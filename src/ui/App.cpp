#include "ui/App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ui/Art.h"

namespace ui {

    namespace {
        using namespace art::palette;

        constexpr int16_t kSceneW = 480;
        constexpr int16_t kGroundY = 152;
        constexpr int16_t kPetFloor = 157;
        constexpr int16_t kDustSlots[4] = {40, 116, 364, 440};
        constexpr uint32_t kGameMs = 20000;

        constexpr App::Screen kHome = App::Screen::Home;

        struct PanelButton {
            art::Icon icon;
            const char* label;
        };
        constexpr PanelButton kPanelButtons[6] = {
            {art::Icon::Feed, "FEED"}, {art::Icon::Play, "PLAY"}, {art::Icon::Clean, "CLEAN"},
            {art::Icon::Light, "LIGHT"}, {art::Icon::Heal, "HEAL"}, {art::Icon::Stats, "STATS"},
        };

        constexpr int16_t panelX(int i) { return 484 + (i % 2) * 78; }
        constexpr int16_t panelY(int i) { return 6 + (i / 2) * 55; }

        uint8_t scaleFor(const game::Pet& pet) {
            switch (pet.stage) {
            case game::Stage::Egg: return 5;
            case game::Stage::Spark: return 6;
            case game::Stage::Twinkle: return 6;
            case game::Stage::Adult: return pet.form == game::Form::Dim ? 5 : 5;
            }
            return 5;
        }

        std::string formatAge(float seconds) {
            const int minutes = static_cast<int>(seconds / 60);
            char buffer[32];
            if (minutes < 60) std::snprintf(buffer, sizeof buffer, "%dm", minutes);
            else if (minutes < 60 * 48) std::snprintf(buffer, sizeof buffer, "%dh %02dm", minutes / 60, minutes % 60);
            else std::snprintf(buffer, sizeof buffer, "%dd %dh", minutes / 1440, (minutes / 60) % 24);
            return buffer;
        }
    } // namespace

    App::App(game::Pet& pet) : pet_(pet) {}

    uint32_t App::random() {
        seed_ ^= seed_ << 13;
        seed_ ^= seed_ >> 17;
        seed_ ^= seed_ << 5;
        return seed_;
    }

    bool App::takeSaveRequest() {
        const bool requested = saveRequested_;
        saveRequested_ = false;
        return requested;
    }

    bool App::hit(const Button& b, int16_t x, int16_t y) {
        return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
    }

    void App::toast(std::string text, uint32_t nowMs, uint32_t durationMs) {
        toast_ = std::move(text);
        toastUntilMs_ = nowMs + durationMs;
    }

    void App::startAnim(Anim anim, uint32_t nowMs, uint32_t durationMs) {
        anim_ = anim;
        animStartMs_ = nowMs;
        animUntilMs_ = nowMs + durationMs;
    }

    void App::act(game::Reaction reaction, Anim success, const char* okText, uint32_t nowMs) {
        switch (reaction) {
        case game::Reaction::Ok:
            startAnim(success, nowMs, 1800);
            if (okText) toast(okText, nowMs);
            saveRequested_ = true;
            break;
        case game::Reaction::Full:
            startAnim(Anim::Refuse, nowMs, 900);
            toast("Too full!", nowMs);
            break;
        case game::Reaction::Asleep:
            toast("Shh... it's sleeping", nowMs);
            break;
        case game::Reaction::NotSick:
            startAnim(Anim::Refuse, nowMs, 900);
            toast("It isn't sick. Yuck!", nowMs);
            saveRequested_ = true;
            break;
        case game::Reaction::Refused:
            startAnim(Anim::Refuse, nowMs, 900);
            toast(pet_.stage == game::Stage::Egg ? "Wait for it to hatch" : "Not now", nowMs);
            break;
        default: break;
        }
    }

    // ------------------------------------------------------------------------------------- update

    void App::update(const Input& input, uint32_t nowMs, float timeScale) {
        if (lastMs_ == 0) {
            lastMs_ = nowMs;
            lastInputMs_ = nowMs;
        }
        const float realDt = std::min(1.0f, static_cast<float>(nowMs - lastMs_) / 1000.0f);
        lastMs_ = nowMs;

        const game::Reaction life = game::tick(pet_, realDt * timeScale);
        if (life == game::Reaction::Hatched) {
            startAnim(Anim::Hatch, nowMs, 1600);
            toast("It hatched!", nowMs, 3000);
            saveRequested_ = true;
        } else if (life == game::Reaction::Evolved) {
            startAnim(Anim::Evolve, nowMs, 2600);
            toast(std::string("Grew into ") + game::stageName(pet_) + "!", nowMs, 3500);
            saveRequested_ = true;
        }

        if (input.bootPressed) {
            lastInputMs_ = nowMs;
            if (screen_ == Screen::Home) screen_ = Screen::Stats;
            else if (screen_ != Screen::Game) screen_ = kHome;
        }

        if (input.touching) {
            lastInputMs_ = nowMs;
            if (!touching_) {
                touching_ = true;
                startX_ = touchX_ = input.x;
                startY_ = touchY_ = input.y;
                touchStartMs_ = nowMs;
                pressed_ = -1;
                if (screen_ != Screen::Game && screen_ != Screen::GameOver)
                    for (int i = 0; i < 6; ++i)
                        if (hit({panelX(i), panelY(i), 74, 51}, input.x, input.y)) pressed_ = static_cast<int8_t>(i);
            }
            touchX_ = input.x;
            touchY_ = input.y;
            onTouchMove(input.x, input.y, nowMs);
        } else if (touching_) {
            touching_ = false;
            const bool still = std::abs(touchX_ - startX_) < 16 && std::abs(touchY_ - startY_) < 16;
            if (still && nowMs - touchStartMs_ < 700) onTap(touchX_, touchY_, nowMs);
            pressed_ = -1;
        }

        if (anim_ != Anim::None && nowMs >= animUntilMs_) anim_ = Anim::None;
        if (screen_ == Screen::Game) updateGame(realDt, nowMs);
        else updateWander(realDt, nowMs);
    }

    void App::updateWander(float dt, uint32_t nowMs) {
        if (nowMs >= nextBlinkMs_) {
            blinkUntilMs_ = nowMs + 140;
            nextBlinkMs_ = nowMs + 2200 + random() % 3000;
        }
        const bool canMove = pet_.stage != game::Stage::Egg && !pet_.asleep && anim_ == Anim::None
                          && screen_ == kHome && !pet_.sick;
        if (canMove && nowMs >= nextWanderMs_) {
            targetX_ = 110 + static_cast<float>(random() % 260);
            nextWanderMs_ = nowMs + 2500 + random() % 4500;
            if (game::mood(pet_) == game::Mood::Happy && random() % 2) hopT_ = 0.0001f;
        }
        if (canMove) {
            const float step = 30 * dt;
            if (std::abs(targetX_ - petX_) > step) {
                facingLeft_ = targetX_ < petX_;
                petX_ += facingLeft_ ? -step : step;
            }
        }
        if (hopT_ > 0) {
            hopT_ += dt / 0.55f;
            if (hopT_ >= 1) hopT_ = 0;
        }
    }

    void App::onTouchMove(int16_t x, int16_t y, uint32_t nowMs) {
        if (screen_ == Screen::Game) {
            catcherX_ = std::clamp<float>(x, 30, 610);
            return;
        }
        if (screen_ == Screen::Clean) {
            for (uint8_t i = 0; i < pet_.dust && i < 4; ++i) {
                if (std::abs(x - kDustSlots[i]) < 30 && y > 110) {
                    game::clean(pet_, 1);
                    dustPoofSlot_ = i;
                    dustPoofUntilMs_ = nowMs + 450;
                    saveRequested_ = true;
                    if (pet_.dust == 0) {
                        screen_ = kHome;
                        startAnim(Anim::Happy, nowMs, 1500);
                        toast("Sparkling clean!", nowMs);
                    }
                    break;
                }
            }
            return;
        }
        if (screen_ != kHome || pet_.stage == game::Stage::Egg) return;
        // Rubbing the pet strokes it.
        const int16_t w = art::petWidth(pet_, scaleFor(pet_)), h = art::petHeight(pet_, scaleFor(pet_));
        const bool onPet = std::abs(x - static_cast<int16_t>(petX_)) < w / 2 && y > kPetFloor - h && y < kPetFloor;
        const bool moved = std::abs(x - startX_) + std::abs(y - startY_) > 10;
        if (onPet && moved && nowMs - lastStrokeMs_ > 700) {
            lastStrokeMs_ = nowMs;
            const game::Reaction r = game::stroke(pet_);
            if (r == game::Reaction::Grumpy) {
                startAnim(Anim::Grumpy, nowMs, 1200);
                toast("Hey! It was sleeping", nowMs);
            } else {
                startAnim(Anim::Happy, nowMs, 1200);
            }
            saveRequested_ = true;
        }
    }

    void App::onTap(int16_t x, int16_t y, uint32_t nowMs) {
        switch (screen_) {
        case Screen::Game:
            return;
        case Screen::GameOver: {
            screen_ = kHome;
            act(game::play(pet_, score_), Anim::Happy, nullptr, nowMs);
            return;
        }
        case Screen::Feed:
            if (hit({80, 64, 140, 62}, x, y)) {
                screen_ = kHome;
                act(game::feedMeal(pet_), Anim::Eat, "Yum! Stardust stew", nowMs);
            } else if (hit({260, 64, 140, 62}, x, y)) {
                screen_ = kHome;
                act(game::feedSnack(pet_), Anim::Snack, "A comet candy!", nowMs);
            } else {
                screen_ = kHome;
            }
            return;
        case Screen::Stats:
            if (hit({318, 128, 140, 28}, x, y)) screen_ = Screen::Confirm;
            else screen_ = kHome;
            return;
        case Screen::Confirm:
            if (hit({110, 104, 110, 30}, x, y)) {
                game::newEgg(pet_, random());
                petX_ = targetX_ = 220;
                screen_ = kHome;
                toast("A new egg appeared", nowMs);
                saveRequested_ = true;
            } else {
                screen_ = kHome;
            }
            return;
        case Screen::Clean:
            if (x >= kSceneW) screen_ = kHome;
            return;
        case Screen::Home: break;
        }

        // Home: panel buttons.
        for (int i = 0; i < 6; ++i) {
            if (!hit({panelX(i), panelY(i), 74, 51}, x, y)) continue;
            switch (i) {
            case 0:
                if (pet_.stage == game::Stage::Egg) act(game::Reaction::Refused, Anim::None, nullptr, nowMs);
                else if (pet_.asleep) act(game::Reaction::Asleep, Anim::None, nullptr, nowMs);
                else screen_ = Screen::Feed;
                break;
            case 1:
                if (pet_.stage == game::Stage::Egg) act(game::Reaction::Refused, Anim::None, nullptr, nowMs);
                else if (pet_.asleep) act(game::Reaction::Asleep, Anim::None, nullptr, nowMs);
                else if (pet_.sick) toast("Too sick to play", nowMs);
                else startGame(nowMs);
                break;
            case 2:
                if (pet_.dust == 0) toast("Nothing to clean", nowMs);
                else {
                    screen_ = Screen::Clean;
                    toast("Swipe the dust bunnies away!", nowMs, 4000);
                }
                break;
            case 3:
                game::toggleLights(pet_);
                toast(pet_.lightsOff ? (pet_.asleep ? "Lights off. Sweet dreams" : "Lights off") : "Lights on", nowMs);
                saveRequested_ = true;
                break;
            case 4:
                act(game::medicine(pet_), Anim::Heal, "Feeling better!", nowMs);
                break;
            case 5:
                screen_ = Screen::Stats;
                break;
            }
            return;
        }

        // Tapping the egg wobbles it; tapping the pet makes it look up happily.
        const int16_t w = art::petWidth(pet_, scaleFor(pet_)), h = art::petHeight(pet_, scaleFor(pet_));
        if (std::abs(x - static_cast<int16_t>(petX_)) < w / 2 && y > kPetFloor - h) {
            if (pet_.stage == game::Stage::Egg) startAnim(Anim::Refuse, nowMs, 500);
            else if (pet_.asleep) toast("Zzz...", nowMs);
            else if (anim_ == Anim::None) hopT_ = 0.0001f;
        }
    }

    void App::startGame(uint32_t nowMs) {
        screen_ = Screen::Game;
        score_ = 0;
        catcherX_ = 320;
        gameEndMs_ = nowMs + kGameMs;
        nextStarMs_ = nowMs + 400;
        for (auto& s: stars_) s.alive = false;
    }

    void App::updateGame(float dt, uint32_t nowMs) {
        if (nowMs >= gameEndMs_) {
            screen_ = Screen::GameOver;
            return;
        }
        if (nowMs >= nextStarMs_) {
            for (auto& s: stars_) {
                if (s.alive) continue;
                s = {20.0f + static_cast<float>(random() % 600), -16.0f, 60.0f + static_cast<float>(random() % 70), true};
                break;
            }
            // Stars come faster as the round goes on.
            const uint32_t left = gameEndMs_ - nowMs;
            nextStarMs_ = nowMs + 380 + left / 40 + random() % 300;
        }
        for (auto& s: stars_) {
            if (!s.alive) continue;
            s.y += s.speed * dt;
            if (s.y > 120 && s.y < 150 && std::abs(s.x - catcherX_) < 38) {
                s.alive = false;
                ++score_;
                dustPoofSlot_ = 9;  // reuse the poof timer for a catch flash
                dustPoofUntilMs_ = nowMs + 250;
            } else if (s.y > 180) {
                s.alive = false;
            }
        }
    }

    // ------------------------------------------------------------------------------------- render

    void App::render(gfx::Canvas& c, uint32_t nowMs) {
        if (screen_ == Screen::Game || screen_ == Screen::GameOver) {
            drawGame(c, nowMs);
            drawToast(c, nowMs);
            return;
        }
        const bool dark = pet_.lightsOff;
        drawSky(c, kSceneW, nowMs, dark);
        drawGround(c, kSceneW, dark);
        for (uint8_t i = 0; i < pet_.dust && i < 4; ++i) art::drawDust(c, kDustSlots[i], kGroundY + 6, nowMs, dark ? 150 : 0);
        if (nowMs < dustPoofUntilMs_ && dustPoofSlot_ < 4) {
            const uint8_t r = static_cast<uint8_t>(3 + (dustPoofUntilMs_ - nowMs) / 60);
            art::drawSparkle(c, kDustSlots[dustPoofSlot_], kGroundY - 4, r, kGold);
        }
        drawPet(c, nowMs, dark);
        drawEffects(c, nowMs);
        drawHud(c, nowMs);
        drawPanel(c);
        if (screen_ == Screen::Feed) drawFeed(c);
        if (screen_ == Screen::Stats) drawStats(c);
        if (screen_ == Screen::Confirm) drawConfirm(c);
        drawToast(c, nowMs);
    }

    void App::drawSky(gfx::Canvas& c, int16_t width, uint32_t nowMs, bool dark) {
        constexpr int kBands = 10;
        for (int i = 0; i < kBands; ++i) {
            uint16_t color = gfx::mix(kSkyTop, kSkyBottom, static_cast<uint8_t>(i * 255 / (kBands - 1)));
            if (dark) color = gfx::mix(color, 0, 170);
            const int16_t y0 = static_cast<int16_t>(i * kGroundY / kBands), y1 = static_cast<int16_t>((i + 1) * kGroundY / kBands);
            c.fillRect(0, y0, width, y1 - y0, color);
        }
        // Twinkling stars at fixed, pseudo-random positions.
        uint32_t s = 0x1234567;
        for (int i = 0; i < 46; ++i) {
            s = s * 1103515245 + 12345;
            const int16_t x = static_cast<int16_t>((s >> 8) % width);
            const int16_t y = static_cast<int16_t>((s >> 20) % (kGroundY - 12));
            const uint32_t phase = (nowMs / 90 + (s & 63)) % 64;
            const uint8_t level = static_cast<uint8_t>(phase < 32 ? phase * 8 : (63 - phase) * 8);
            const uint16_t color = gfx::mix(gfx::rgb(90, 96, 140), gfx::rgb(255, 250, 230), dark ? 255 : level);
            c.pixel(x, y, color);
            if ((s & 7) == 0) art::drawSparkle(c, x, y, level > 160 ? 2 : 1, color);
        }
        if (dark) {
            // A crescent moon when the lights are out.
            c.disc(400, 34, 14, gfx::rgb(250, 240, 200));
            c.disc(407, 30, 13, gfx::mix(kSkyTop, 0, 170));
        }
    }

    void App::drawGround(gfx::Canvas& c, int16_t width, bool dark) {
        const auto d = [dark](uint16_t col) { return dark ? gfx::mix(col, 0, 150) : col; };
        c.fillRect(0, kGroundY, width, gfx::kHeight - kGroundY, d(kGround));
        c.fillRect(0, kGroundY, width, 2, d(kGroundLight));
        constexpr int16_t kCraters[][3] = {{30, 162, 10}, {150, 166, 6}, {270, 160, 14}, {410, 165, 8}, {470, 158, 5}};
        for (const auto& k: kCraters) {
            c.fillRect(k[0], k[1], k[2], 3, d(kGroundDark));
            c.fillRect(k[0] + 2, k[1] + 3, k[2] - 4, 2, d(kGroundDark));
        }
    }

    void App::drawPet(gfx::Canvas& c, uint32_t nowMs, bool dark) {
        art::Look look;
        look.scale = scaleFor(pet_);
        look.x = static_cast<int16_t>(petX_);
        look.y = kPetFloor;
        look.flip = facingLeft_;
        look.dim = dark ? 140 : 0;

        art::Face face;
        const game::Mood mood = game::mood(pet_);
        switch (mood) {
        case game::Mood::Asleep: face = {art::Eyes::Closed, art::Mouth::Flat, false}; break;
        case game::Mood::Sick: face = {art::Eyes::Sad, art::Mouth::Frown, false}; break;
        case game::Mood::Hungry: face = {art::Eyes::Sad, art::Mouth::Open, false}; break;
        case game::Mood::Sleepy: face = {art::Eyes::Blink, art::Mouth::Flat, false}; break;
        case game::Mood::Dusty: face = {art::Eyes::Sad, art::Mouth::Flat, false}; break;
        case game::Mood::Bored: face = {art::Eyes::Open, art::Mouth::Flat, false}; break;
        case game::Mood::Happy: face = {art::Eyes::Open, art::Mouth::Smile, true}; break;
        case game::Mood::Content: face = {art::Eyes::Open, art::Mouth::Smile, false}; break;
        }
        if (pet_.form == game::Form::Dim && mood == game::Mood::Content) face = {art::Eyes::Sad, art::Mouth::Flat, false};

        const float t = static_cast<float>(nowMs - animStartMs_) / 1000.0f;
        switch (anim_) {
        case Anim::Eat:
        case Anim::Snack: face = {art::Eyes::Happy, (nowMs / 200) % 2 ? art::Mouth::Chew : art::Mouth::Flat, true}; break;
        case Anim::Happy:
        case Anim::Heal:
        case Anim::Evolve:
        case Anim::Hatch: face = {art::Eyes::Happy, art::Mouth::Open, true}; break;
        case Anim::Refuse:
            face = {art::Eyes::Sad, art::Mouth::Frown, false};
            look.x += static_cast<int16_t>(std::sin(t * 30) * 4);
            break;
        case Anim::Grumpy: face = {art::Eyes::Sad, art::Mouth::Frown, false}; break;
        default:
            if (face.eyes == art::Eyes::Open && nowMs < blinkUntilMs_) face.eyes = art::Eyes::Blink;
        }

        // Gentle breathing bob, plus hops.
        if (!pet_.asleep && pet_.stage != game::Stage::Egg) look.y -= ((nowMs / 450) % 2) ? 0 : 2;
        if (hopT_ > 0) {
            look.y -= static_cast<int16_t>(std::sin(hopT_ * 3.14159f) * 22);
            look.squash = hopT_ < 0.12f || hopT_ > 0.9f ? 1 : -1;
        }
        if (pet_.stage == game::Stage::Egg) {
            look.crack = static_cast<uint8_t>(std::min(3.0f, pet_.stageSeconds / (game::tuning::kHatchSeconds / 3.5f)));
            // Eggs about to hatch rock from side to side.
            if (look.crack >= 2 && (nowMs / 300) % 4 == 0) look.x += ((nowMs / 100) % 2) ? 2 : -2;
        }
        const int16_t h = art::drawPet(c, pet_, look, face, nowMs);

        // Attention bubble.
        if (game::needsAttention(pet_) && (nowMs / 500) % 2 == 0 && screen_ == kHome) {
            const int16_t bx = look.x + art::petWidth(pet_, look.scale) / 2 - 2, by = look.y - h - 4;
            c.roundBox(bx, by, 18, 20, 3, kText);
            c.fillRect(bx + 2, by + 18, 5, 4, kText);
            c.text(bx + 6, by + 3, "!", 2, kAccent);
        }
        if (pet_.asleep) {
            const int16_t zx = look.x + 30, zy = look.y - h - 8;
            const int phase = static_cast<int>((nowMs / 600) % 3);
            for (int i = 0; i <= phase; ++i) c.text(zx + i * 12, zy - i * 12, "z", static_cast<uint8_t>(1 + i), gfx::rgb(200, 210, 255));
        }
        if (pet_.sick && !pet_.asleep) {
            const int16_t sx = look.x - art::petWidth(pet_, look.scale) / 2 - 4, sy = look.y - h + 8 + static_cast<int16_t>((nowMs / 300) % 4);
            c.fillRect(sx, sy, 4, 6, kBlue);
            c.fillRect(sx + 1, sy - 2, 2, 2, kBlue);
        }
    }

    void App::drawEffects(gfx::Canvas& c, uint32_t nowMs) {
        if (anim_ == Anim::None) return;
        const float t = std::clamp(static_cast<float>(nowMs - animStartMs_) / static_cast<float>(animUntilMs_ - animStartMs_), 0.0f, 1.0f);
        const int16_t px = static_cast<int16_t>(petX_);
        const int16_t top = kPetFloor - art::petHeight(pet_, scaleFor(pet_));
        switch (anim_) {
        case Anim::Eat:
        case Anim::Snack: {
            // Food next to the mouth, getting smaller with each bite.
            const int16_t fx = facingLeft_ ? px - 46 : px + 30, fy = kPetFloor - 34;
            const uint8_t bites = static_cast<uint8_t>(t * 4);
            if (bites < 4) {
                const uint8_t s = static_cast<uint8_t>(3 - std::min<uint8_t>(bites, 2));
                art::drawIcon(c, anim_ == Anim::Eat ? art::Icon::Feed : art::Icon::Star, fx, fy, s, anim_ == Anim::Eat ? kGold : kPink);
            }
            break;
        }
        case Anim::Happy:
        case Anim::Play:
            for (int i = 0; i < 3; ++i) {
                const float k = std::fmod(t + i * 0.33f, 1.0f);
                art::drawIcon(c, art::Icon::Heart, static_cast<int16_t>(px - 30 + i * 24), static_cast<int16_t>(top - 4 - k * 36), 2,
                              gfx::mix(kPink, kSkyTop, static_cast<uint8_t>(k * 200)));
            }
            break;
        case Anim::Heal:
            art::drawIcon(c, art::Icon::Heal, px - 12, static_cast<int16_t>(top - 10 - t * 30), 3, kGreen);
            break;
        case Anim::Hatch:
        case Anim::Evolve: {
            if (t < 0.25f) c.fillRect(0, 0, kSceneW, kGroundY, gfx::mix(kText, kSkyTop, static_cast<uint8_t>(t * 4 * 255)));
            for (int i = 0; i < 8; ++i) {
                const float a = i * 0.785f + t * 2;
                const float r = 30 + t * 50;
                art::drawSparkle(c, static_cast<int16_t>(px + std::cos(a) * r), static_cast<int16_t>(kPetFloor - 45 + std::sin(a) * r * 0.6f),
                                 3, i % 2 ? kGold : kText);
            }
            break;
        }
        default: break;
        }
    }

    void App::drawHud(gfx::Canvas& c, uint32_t nowMs) {
        (void) nowMs;
        const uint16_t text = pet_.lightsOff ? kMuted : kText;
        int16_t x = 10 + c.text(10, 8, game::stageName(pet_), 2, text);
        if (pet_.stage != game::Stage::Egg) c.text(x + 6, 15, game::moodName(game::mood(pet_)), 1, kMuted);
        if (pet_.stage == game::Stage::Egg) return;
        struct Meter { art::Icon icon; float value; uint16_t color; };
        const Meter meters[4] = {
            {art::Icon::Feed, pet_.hunger, kGold}, {art::Icon::Heart, pet_.joy, kPink},
            {art::Icon::Bolt, pet_.energy, kBlue}, {art::Icon::Heal, pet_.health, kGreen},
        };
        for (int i = 0; i < 4; ++i) {
            const int16_t mx = 10 + i * 62, my = 28;
            art::drawIcon(c, meters[i].icon, mx, my, 1, meters[i].color);
            const int filled = static_cast<int>(std::ceil(meters[i].value / 20.0f));
            for (int p = 0; p < 5; ++p)
                c.fillRect(mx + 12 + p * 8, my + 1, 6, 6, p < filled ? meters[i].color : gfx::rgb(60, 62, 90));
        }
    }

    void App::drawButton(gfx::Canvas& c, const Button& b, int icon, const char* label, bool hot, bool enabled) {
        c.roundBox(b.x, b.y, b.w, b.h, 4, hot ? kButtonHot : kButton);
        const uint16_t color = enabled ? kText : gfx::rgb(110, 102, 96);
        if (icon >= 0) {
            art::drawIcon(c, static_cast<art::Icon>(icon), b.x + b.w / 2 - 12, b.y + 7, 3, enabled ? kAccent : color);
            c.textCentered(b.x + b.w / 2, b.y + b.h - 12, label, 1, color);
        } else {
            c.textCentered(b.x + b.w / 2, b.y + b.h / 2 - 7, label, 2, color);
        }
    }

    void App::drawPanel(gfx::Canvas& c) {
        c.fillRect(kSceneW, 0, gfx::kWidth - kSceneW, gfx::kHeight, kPanel);
        for (int i = 0; i < 6; ++i) {
            bool enabled = true;
            if (i == 2) enabled = pet_.dust > 0;
            if (i == 4) enabled = pet_.sick;
            if ((i == 0 || i == 1) && (pet_.stage == game::Stage::Egg || pet_.asleep)) enabled = false;
            drawButton(c, {panelX(i), panelY(i), 74, 51}, static_cast<int>(kPanelButtons[i].icon),
                       screen_ == Screen::Clean && i == 2 ? "DONE" : kPanelButtons[i].label, pressed_ == i, enabled);
        }
    }

    void App::drawFeed(gfx::Canvas& c) {
        c.roundBox(60, 34, 360, 104, 6, kPanel);
        c.textCentered(240, 44, "What should it eat?", 1, kMuted);
        drawButton(c, {80, 64, 140, 62}, -1, "", false);
        art::drawIcon(c, art::Icon::Feed, 96, 77, 4, kGold);
        c.text(136, 80, "MEAL", 2, kText);
        c.text(136, 100, "fills up", 1, kMuted);
        drawButton(c, {260, 64, 140, 62}, -1, "", false);
        art::drawIcon(c, art::Icon::Star, 276, 77, 4, kPink);
        c.text(316, 80, "SNACK", 2, kText);
        c.text(316, 100, "+joy", 1, kMuted);
    }

    void App::drawStats(gfx::Canvas& c) {
        c.roundBox(8, 6, 464, 160, 6, kPanel);
        int16_t x = 22 + c.text(22, 16, game::stageName(pet_), 2, kText);
        char line[64];
        std::snprintf(line, sizeof line, "gen %lu", static_cast<unsigned long>(pet_.generation));
        c.text(x + 10, 23, line, 1, kMuted);
        struct Bar { const char* label; float value; uint16_t color; };
        const Bar bars[4] = {{"FOOD", pet_.hunger, kGold}, {"JOY", pet_.joy, kPink}, {"ENERGY", pet_.energy, kBlue}, {"HEALTH", pet_.health, kGreen}};
        for (int i = 0; i < 4; ++i) {
            const int16_t y = 44 + i * 22;
            c.text(22, y, bars[i].label, 1, kMuted);
            c.fillRect(80, y - 1, 180, 9, gfx::rgb(50, 46, 42));
            c.fillRect(80, y - 1, static_cast<int16_t>(180 * bars[i].value / 100), 9, bars[i].color);
        }
        const std::string age = "Age " + formatAge(pet_.ageSeconds);
        c.text(290, 44, age, 1, kText);
        std::snprintf(line, sizeof line, "Weight %dg", static_cast<int>(pet_.weight));
        c.text(290, 62, line, 1, kText);
        std::snprintf(line, sizeof line, "Care mistakes %u", pet_.careMistakes);
        c.text(290, 80, line, 1, pet_.careMistakes > 3 ? kBad : kText);
        std::snprintf(line, sizeof line, "Meals %u  Plays %u", pet_.meals, pet_.plays);
        c.text(290, 98, line, 1, kMuted);
        drawButton(c, {318, 128, 140, 28}, -1, "", false);
        c.textCentered(388, 138, "NEW EGG", 1, kText);
        c.text(22, 140, "Tap anywhere to close", 1, gfx::rgb(110, 102, 96));
    }

    void App::drawConfirm(gfx::Canvas& c) {
        c.roundBox(80, 36, 320, 108, 6, kPanel);
        c.textCentered(240, 52, "Start over with a new egg?", 1, kText);
        c.textCentered(240, 70, "Your starling will fly away.", 1, kMuted);
        drawButton(c, {110, 104, 110, 30}, -1, "", false);
        c.textCentered(165, 115, "YES", 1, kBad);
        drawButton(c, {260, 104, 110, 30}, -1, "", false);
        c.textCentered(315, 115, "KEEP IT", 1, kText);
    }

    void App::drawGame(gfx::Canvas& c, uint32_t nowMs) {
        drawSky(c, gfx::kWidth, nowMs, false);
        drawGround(c, gfx::kWidth, false);
        for (const auto& s: stars_)
            if (s.alive) art::drawIcon(c, art::Icon::Star, static_cast<int16_t>(s.x - 8), static_cast<int16_t>(s.y - 8), 2, kGold);
        art::Look look;
        look.scale = pet_.stage == game::Stage::Adult ? 3 : 4;
        look.x = static_cast<int16_t>(catcherX_);
        look.y = kPetFloor;
        art::Face face{art::Eyes::Open, art::Mouth::Open, true};
        if (nowMs < dustPoofUntilMs_) face.eyes = art::Eyes::Happy;
        art::drawPet(c, pet_, look, face, nowMs);
        char line[48];
        std::snprintf(line, sizeof line, "STARS %d", score_);
        c.text(12, 10, line, 2, kText);
        if (screen_ == Screen::Game) {
            std::snprintf(line, sizeof line, "%lus", static_cast<unsigned long>((gameEndMs_ - nowMs + 999) / 1000));
            c.text(gfx::kWidth - 12 - gfx::Canvas::textWidth(line, 2), 10, line, 2, kAccent);
            if (gameEndMs_ - nowMs > kGameMs - 2500) c.textCentered(320, 40, "Drag to catch the falling stars!", 1, kMuted);
        } else {
            c.roundBox(170, 40, 300, 86, 6, kPanel);
            std::snprintf(line, sizeof line, "Caught %d stars!", score_);
            c.textCentered(320, 58, line, 2, kGold);
            c.textCentered(320, 96, "Tap to continue", 1, kMuted);
        }
    }

    void App::drawToast(gfx::Canvas& c, uint32_t nowMs) {
        if (toast_.empty() || nowMs >= toastUntilMs_) return;
        const int16_t width = gfx::Canvas::textWidth(toast_, 1) + 20;
        // Top-right of the sky, clear of the pet and the meters.
        const bool game = screen_ == Screen::Game || screen_ == Screen::GameOver;
        const int16_t right = game ? gfx::kWidth / 2 + width / 2 : kSceneW - 8;
        const int16_t top = game ? 150 : 46;
        c.roundBox(right - width, top, width, 18, 3, gfx::rgb(10, 10, 14));
        c.text(right - width + 10, top + 5, toast_, 1, kText);
    }

} // namespace ui
