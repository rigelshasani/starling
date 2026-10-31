// Screens, touch handling and animation for Starling. Platform independent: the device and the
// simulator feed it input and a clock, and it draws into a gfx::Canvas.
#pragma once

#include <cstdint>
#include <string>

#include "game/Pet.h"
#include "gfx/Canvas.h"

namespace ui {

    struct Input {
        bool touching = false;
        int16_t x = 0;  // logical landscape coordinates, 0..639 x 0..171
        int16_t y = 0;
        bool bootPressed = false;  // short press this frame
    };

    class App {
    public:
        explicit App(game::Pet& pet);

        // Advance animation and game time. timeScale > 1 speeds up the pet's clock (simulator only).
        void update(const Input& input, uint32_t nowMs, float timeScale = 1.0f);
        void render(gfx::Canvas& canvas, uint32_t nowMs);

        // True once after anything worth saving changed.
        bool takeSaveRequest();
        // Milliseconds since the last touch or button press (for dimming).
        uint32_t idleMs(uint32_t nowMs) const { return nowMs - lastInputMs_; }
        void noteActivity(uint32_t nowMs) { lastInputMs_ = nowMs; }
        // Set a short status line (e.g. battery warnings) shown as a toast.
        void toast(std::string text, uint32_t nowMs, uint32_t durationMs = 2500);

        enum class Screen : uint8_t { Home, Feed, Clean, Game, GameOver, Stats, Confirm };
        Screen screen() const { return screen_; }

    private:
        enum class Anim : uint8_t { None, Eat, Snack, Happy, Refuse, Grumpy, Hatch, Evolve, Heal, Play };
        struct Star { float x, y, speed; bool alive; };
        struct Button { int16_t x, y, w, h; };

        void onTap(int16_t x, int16_t y, uint32_t nowMs);
        void onTouchMove(int16_t x, int16_t y, uint32_t nowMs);
        void act(game::Reaction reaction, Anim success, const char* okText, uint32_t nowMs);
        void startAnim(Anim anim, uint32_t nowMs, uint32_t durationMs);
        void startGame(uint32_t nowMs);
        void updateWander(float dt, uint32_t nowMs);
        void updateGame(float dt, uint32_t nowMs);

        void drawSky(gfx::Canvas& c, int16_t width, uint32_t nowMs, bool dark);
        void drawGround(gfx::Canvas& c, int16_t width, bool dark);
        void drawHud(gfx::Canvas& c, uint32_t nowMs);
        void drawPanel(gfx::Canvas& c);
        void drawPet(gfx::Canvas& c, uint32_t nowMs, bool dark);
        void drawEffects(gfx::Canvas& c, uint32_t nowMs);
        void drawFeed(gfx::Canvas& c);
        void drawStats(gfx::Canvas& c);
        void drawConfirm(gfx::Canvas& c);
        void drawGame(gfx::Canvas& c, uint32_t nowMs);
        void drawToast(gfx::Canvas& c, uint32_t nowMs);
        void drawButton(gfx::Canvas& c, const Button& b, int icon, const char* label, bool hot, bool enabled = true);

        static bool hit(const Button& b, int16_t x, int16_t y);
        uint32_t random();

        game::Pet& pet_;
        Screen screen_ = Screen::Home;
        uint32_t lastMs_ = 0;
        uint32_t lastInputMs_ = 0;
        bool saveRequested_ = false;
        uint32_t seed_ = 0xC0FFEE;

        // Touch tracking.
        bool touching_ = false;
        int16_t touchX_ = 0, touchY_ = 0, startX_ = 0, startY_ = 0;
        uint32_t touchStartMs_ = 0, lastStrokeMs_ = 0;
        int8_t pressed_ = -1;

        // Pet motion on the home screen.
        float petX_ = 220, targetX_ = 220, hopT_ = 0;
        bool facingLeft_ = false;
        uint32_t nextWanderMs_ = 0, nextBlinkMs_ = 0, blinkUntilMs_ = 0;

        Anim anim_ = Anim::None;
        uint32_t animStartMs_ = 0, animUntilMs_ = 0;
        std::string toast_;
        uint32_t toastUntilMs_ = 0;
        uint8_t dustPoofSlot_ = 0;
        uint32_t dustPoofUntilMs_ = 0;

        // Star catch.
        static constexpr int kMaxStars = 12;
        Star stars_[kMaxStars] = {};
        float catcherX_ = 320;
        int score_ = 0;
        uint32_t gameEndMs_ = 0, nextStarMs_ = 0;
    };

} // namespace ui
