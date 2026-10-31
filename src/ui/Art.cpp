#include "ui/Art.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace art {

    namespace {

        // Sprite pixels: '.' clear, 'k' outline, 'b' body, 'l' highlight, 'a' accent.
        // Body pixels touching the outline on the right or below are shaded automatically.
        struct Sprite {
            const char* const* rows;
            int8_t w, h;
            int8_t eyeY, eyeL, eyeR;  // top-left of each 2x2 eye
            int8_t mouthX, mouthY;    // left of the 2-pixel mouth
            int8_t cheekY, cheekL, cheekR;
        };

        constexpr const char* kEggRows[] = {
            "....kkkk....",
            "...kllbbk...",
            "..kllbbbbk..",
            "..klbbbbbk..",
            ".kbbbabbbbk.",
            ".kbbaaabbbk.",
            ".kbbbabbbbk.",
            ".kbbbbbbbbk.",
            ".kbbbbbbabk.",
            ".kbbbbbaaak.",
            ".kbbbbbbabk.",
            "..kbbbbbbk..",
            "..kbbbbbbk..",
            "...kbbbbk...",
            "....kkkk....",
        };
        constexpr const char* kSparkRows[] = {
            "..k......k..",
            ".kak....kak.",
            ".kbbkkkkbbk.",
            "kbllbbbbbbbk",
            "kbllbbbbbbbk",
            "kblbbbbbbbbk",
            "kbbbbbbbbbbk",
            "kbbbbbbbbbbk",
            ".kbbbbbbbbk.",
            "..kkkkkkkk..",
        };
        constexpr const char* kTwinkleRows[] = {
            ".......kk.......",
            "......kbbk......",
            "......kbbk......",
            ".....kblbbk.....",
            "kkkkkkbbbbkkkkkk",
            "kbbllbbbbbbbbbbk",
            ".kbllbbbbbbbbbk.",
            "..kbbbbbbbbbbk..",
            "...kbbbbbbbbk...",
            "...kbbbbbbbbk...",
            "..kbbbbbbbbbbk..",
            "..kbbbbkkbbbbk..",
            ".kbbbbk..kbbbbk.",
            ".kbbbk....kbbbk.",
            ".kkkk......kkkk.",
        };
        constexpr const char* kAdultRows[] = {
            ".........kk.........",
            "........kbbk........",
            "........kbbk........",
            ".......kblbbk.......",
            ".......kblbbk.......",
            "kkkkkkkbbbbbbkkkkkkk",
            "kbllbbbbbbbbbbbbbbbk",
            ".kbllbbbbbbbbbbbbbk.",
            "..kblbbbbbbbbbbbbk..",
            "...kbbbbbbbbbbbbk...",
            "....kbbbbbbbbbbk....",
            "....kbbbbbbbbbbk....",
            "...kbbbbbbbbbbbbk...",
            "...kbbbbbbbbbbbbk...",
            "..kbbbbbbkkbbbbbbk..",
            "..kbbbbbk..kbbbbbk..",
            ".kbbbbbk....kbbbbbk.",
            ".kbbbbk......kbbbbk.",
            ".kkkkk........kkkkk.",
        };

        constexpr Sprite kEgg{kEggRows, 12, 15, -1, 0, 0, 0, 0, -1, 0, 0};
        constexpr Sprite kSpark{kSparkRows, 12, 10, 5, 3, 7, 5, 7, 6, 2, 9};
        constexpr Sprite kTwinkle{kTwinkleRows, 16, 15, 7, 5, 9, 7, 9, 8, 4, 11};
        constexpr Sprite kAdult{kAdultRows, 20, 19, 9, 7, 11, 9, 11, 10, 6, 13};

        struct Colors {
            uint16_t outline, body, light, shade, accent, cheek;
        };

        constexpr Colors kEggColors{gfx::rgb(74, 63, 53), gfx::rgb(243, 237, 226), gfx::rgb(255, 255, 255),
                                    gfx::rgb(214, 204, 188), palette::kAccent, palette::kPink};
        constexpr Colors kGoldColors{gfx::rgb(90, 58, 26), gfx::rgb(255, 201, 74), gfx::rgb(255, 241, 176),
                                     gfx::rgb(232, 163, 58), palette::kAccent, gfx::rgb(255, 143, 163)};
        constexpr Colors kNovaColors{gfx::rgb(96, 60, 20), gfx::rgb(255, 217, 94), gfx::rgb(255, 250, 214),
                                     gfx::rgb(240, 172, 60), gfx::rgb(255, 246, 200), gfx::rgb(255, 143, 163)};
        constexpr Colors kCometColors{gfx::rgb(30, 58, 95), gfx::rgb(143, 211, 255), gfx::rgb(226, 246, 255),
                                      gfx::rgb(77, 166, 224), gfx::rgb(255, 255, 255), gfx::rgb(255, 160, 200)};
        constexpr Colors kNebulaColors{gfx::rgb(59, 34, 102), gfx::rgb(199, 155, 255), gfx::rgb(235, 221, 255),
                                       gfx::rgb(154, 106, 224), gfx::rgb(255, 190, 240), gfx::rgb(255, 150, 210)};
        constexpr Colors kDimColors{gfx::rgb(46, 42, 38), gfx::rgb(154, 148, 140), gfx::rgb(200, 194, 186),
                                    gfx::rgb(110, 104, 98), gfx::rgb(180, 174, 166), gfx::rgb(190, 150, 150)};

        const Sprite& spriteFor(const game::Pet& pet) {
            switch (pet.stage) {
            case game::Stage::Egg: return kEgg;
            case game::Stage::Spark: return kSpark;
            case game::Stage::Twinkle: return kTwinkle;
            case game::Stage::Adult: return pet.form == game::Form::Dim ? kTwinkle : kAdult;
            }
            return kSpark;
        }

        Colors colorsFor(const game::Pet& pet) {
            if (pet.stage == game::Stage::Egg) return kEggColors;
            if (pet.stage != game::Stage::Adult) return kGoldColors;
            switch (pet.form) {
            case game::Form::Nova: return kNovaColors;
            case game::Form::Comet: return kCometColors;
            case game::Form::Nebula: return kNebulaColors;
            case game::Form::Dim: return kDimColors;
            default: return kGoldColors;
            }
        }

        // Maps art pixels to screen rectangles, applying scale, squash and mirroring.
        struct Grid {
            int16_t left, top, wTotal, hTotal;
            int8_t w, h;
            bool flip;
            int16_t x0(int c) const { int cc = flip ? w - 1 - c : c; return left + (cc * wTotal) / w; }
            int16_t x1(int c) const { int cc = flip ? w - 1 - c : c; return left + ((cc + 1) * wTotal) / w; }
            int16_t y0(int r) const { return top + (r * hTotal) / h; }
            int16_t y1(int r) const { return top + ((r + 1) * hTotal) / h; }
            void cell(gfx::Canvas& c, int col, int row, uint16_t color) const {
                if (col < 0 || row < 0 || col >= w || row >= h) {
                    // Allow drawing just outside the sprite (accessories) with the same cell size.
                    const int16_t cw = wTotal / w, ch = hTotal / h;
                    const int cc = flip ? w - 1 - col : col;
                    c.fillRect(left + cc * cw, top + row * ch, cw, ch, color);
                    return;
                }
                const int16_t a = x0(col), b = x1(col);
                c.fillRect(std::min(a, b), y0(row), std::abs(b - a), y1(row) - y0(row), color);
            }
        };

        uint16_t darken(uint16_t color, uint8_t dim) { return dim ? gfx::mix(color, 0, dim) : color; }

        void drawFace(gfx::Canvas& c, const Grid& g, const Sprite& s, const Colors& col, const Face& face, uint8_t dim) {
            const uint16_t ink = darken(col.outline, dim);
            const uint16_t glint = darken(gfx::rgb(255, 255, 255), dim);
            for (int8_t ex: {s.eyeL, s.eyeR}) {
                const int ey = s.eyeY;
                switch (face.eyes) {
                case Eyes::Open:
                    g.cell(c, ex, ey, ink); g.cell(c, ex + 1, ey, ink);
                    g.cell(c, ex, ey + 1, ink); g.cell(c, ex + 1, ey + 1, ink);
                    // A small glint makes the eyes read as eyes even at 5x.
                    c.fillRect(g.x0(ex) + 1 + (g.flip ? (g.x1(ex) - g.x0(ex)) / 2 - 2 : 0), g.y0(ey) + 1,
                               std::max(1, (g.y1(ey) - g.y0(ey)) / 2), std::max(1, (g.y1(ey) - g.y0(ey)) / 2), glint);
                    break;
                case Eyes::Blink:
                case Eyes::Closed:
                    g.cell(c, ex, ey + 1, ink); g.cell(c, ex + 1, ey + 1, ink);
                    break;
                case Eyes::Happy:
                    g.cell(c, ex, ey + 1, ink); g.cell(c, ex + 1, ey, ink); g.cell(c, ex + 2, ey + 1, ink);
                    break;
                case Eyes::Sad:
                    g.cell(c, ex, ey + 1, ink); g.cell(c, ex + 1, ey + 1, ink);
                    g.cell(c, ex == s.eyeL ? ex + 1 : ex, ey - 1, ink);
                    break;
                case Eyes::Dizzy:
                    g.cell(c, ex, ey, ink); g.cell(c, ex + 1, ey + 1, ink);
                    break;
                }
            }
            const int mx = s.mouthX, my = s.mouthY;
            switch (face.mouth) {
            case Mouth::Smile:
                g.cell(c, mx - 1, my, ink); g.cell(c, mx, my + 1, ink); g.cell(c, mx + 1, my + 1, ink); g.cell(c, mx + 2, my, ink);
                break;
            case Mouth::Open:
            case Mouth::Chew:
                g.cell(c, mx, my, ink); g.cell(c, mx + 1, my, ink);
                g.cell(c, mx, my + 1, darken(palette::kPink, dim)); g.cell(c, mx + 1, my + 1, darken(palette::kPink, dim));
                break;
            case Mouth::Flat:
                g.cell(c, mx, my + 1, ink); g.cell(c, mx + 1, my + 1, ink);
                break;
            case Mouth::Frown:
                g.cell(c, mx - 1, my + 1, ink); g.cell(c, mx, my, ink); g.cell(c, mx + 1, my, ink); g.cell(c, mx + 2, my + 1, ink);
                break;
            }
            if (face.blush && s.cheekY >= 0) {
                g.cell(c, s.cheekL, s.cheekY, darken(col.cheek, dim));
                g.cell(c, s.cheekR, s.cheekY, darken(col.cheek, dim));
            }
        }

        void drawCracks(gfx::Canvas& c, const Grid& g, uint8_t crack, uint16_t ink) {
            // Zigzags appear row by row as the egg gets ready to hatch.
            static constexpr int8_t kZig[3][6][2] = {
                {{3, 7}, {4, 6}, {5, 7}, {6, 6}, {7, 7}, {8, 6}},
                {{3, 10}, {4, 11}, {5, 10}, {6, 11}, {7, 10}, {8, 11}},
                {{5, 3}, {6, 4}, {6, 2}, {5, 3}, {6, 4}, {6, 2}},
            };
            for (uint8_t i = 0; i < crack && i < 3; ++i)
                for (const auto& p: kZig[i]) g.cell(c, p[0], p[1], ink);
        }

        void drawAccessory(gfx::Canvas& c, const game::Pet& pet, const Grid& g, const Colors& col, uint32_t ms, uint8_t dim) {
            if (pet.stage != game::Stage::Adult) return;
            const float t = static_cast<float>(ms) / 1000.0f;
            switch (pet.form) {
            case game::Form::Nova: {
                // Halo floating above the top point.
                const int lift = (static_cast<int>(t * 2) % 2);
                const uint16_t ring = darken(col.accent, dim);
                for (int x = 6; x <= 13; ++x) g.cell(c, x, -3 - lift, ring);
                g.cell(c, 5, -2 - lift, ring); g.cell(c, 14, -2 - lift, ring);
                for (int x = 6; x <= 13; ++x) g.cell(c, x, -1 - lift, darken(gfx::mix(col.accent, col.shade, 120), dim));
                break;
            }
            case game::Form::Comet: {
                // Tail streaming behind (opposite the facing direction).
                for (int i = 0; i < 3; ++i) {
                    const int len = 6 - i * 2 + (static_cast<int>(t * 6 + i) % 2);
                    const uint16_t shade = darken(gfx::mix(col.accent, col.body, static_cast<uint8_t>(60 + i * 60)), dim);
                    for (int k = 1; k <= len; ++k) g.cell(c, -k, 6 + i * 2, k > len - 2 ? darken(col.light, dim) : shade);
                }
                break;
            }
            case game::Form::Nebula: {
                // Soft motes drifting around the body.
                for (int i = 0; i < 3; ++i) {
                    const float a = t * 1.3f + i * 2.094f;
                    const int mx = 10 + static_cast<int>(std::cos(a) * 12), my = 9 + static_cast<int>(std::sin(a) * 6);
                    const uint16_t mote = darken(i == 1 ? col.accent : col.light, dim);
                    g.cell(c, mx, my, mote);
                    if (std::sin(a) < 0) g.cell(c, mx + 1, my, mote);
                }
                break;
            }
            default: break;
            }
        }

    } // namespace

    int16_t petWidth(const game::Pet& pet, uint8_t scale) { return spriteFor(pet).w * scale; }
    int16_t petHeight(const game::Pet& pet, uint8_t scale) { return spriteFor(pet).h * scale; }

    int16_t drawPet(gfx::Canvas& c, const game::Pet& pet, const Look& look, const Face& face, uint32_t ms) {
        const Sprite& s = spriteFor(pet);
        Colors col = colorsFor(pet);
        if (pet.sick) {
            col.body = gfx::mix(col.body, palette::kGreen, 110);
            col.light = gfx::mix(col.light, palette::kGreen, 80);
        }
        const int16_t wTotal = s.w * look.scale + look.squash * look.scale;
        const int16_t hTotal = s.h * look.scale - look.squash * look.scale;
        const Grid g{static_cast<int16_t>(look.x - wTotal / 2), static_cast<int16_t>(look.y - hTotal), wTotal, hTotal, s.w, s.h, look.flip};

        drawAccessory(c, pet, g, col, ms, look.dim);
        for (int r = 0; r < s.h; ++r) {
            for (int x = 0; x < s.w; ++x) {
                const char p = s.rows[r][x];
                if (p == '.') continue;
                uint16_t color = col.body;
                switch (p) {
                case 'k': color = col.outline; break;
                case 'l': color = col.light; break;
                case 'a': color = col.accent; break;
                default: {
                    const char right = x + 1 < s.w ? s.rows[r][x + 1] : '.';
                    const char below = r + 1 < s.h ? s.rows[r + 1][x] : '.';
                    if (right == 'k' || below == 'k') color = col.shade;
                }
                }
                g.cell(c, x, r, darken(color, look.dim));
            }
        }
        if (pet.stage == game::Stage::Egg) drawCracks(c, g, look.crack, darken(col.outline, look.dim));
        else drawFace(c, g, s, col, face, look.dim);
        return hTotal;
    }

    namespace {
        struct IconBits {
            Icon icon;
            const char* rows[8];
        };
        constexpr IconBits kIcons[] = {
            {Icon::Feed, {"..x..x..", "...x..x.", "..x..x..", "xxxxxxxx", "x......x", ".x....x.", "..xxxx..", "........"}},
            {Icon::Play, {"..xxxx..", ".x.xx.x.", "xxx..xxx", "x..xx..x", "x..xx..x", "xxx..xxx", ".x.xx.x.", "..xxxx.."}},
            {Icon::Clean, {".....xx.", "....xx..", "...xx...", "..xx....", ".xxxx...", "xxxxx...", "x.x.x...", "x.x.x..."}},
            {Icon::Light, {"..xxx...", ".xx.....", "xx......", "xx......", "xx......", "xx.....x", ".xx..xx.", "..xxxx.."}},
            {Icon::Heal, {"..xxxx..", "..x..x..", "xxx..xxx", "x......x", "x......x", "xxx..xxx", "..x..x..", "..xxxx.."}},
            {Icon::Stats, {"......x.", "......x.", "...x..x.", "...x..x.", "x..x..x.", "x..x..x.", "x..x..x.", "xxxxxxxx"}},
            {Icon::Heart, {".xx..xx.", "xxxxxxxx", "xxxxxxxx", "xxxxxxxx", ".xxxxxx.", "..xxxx..", "...xx...", "........"}},
            {Icon::Star, {"...xx...", "...xx...", "xxxxxxxx", ".xxxxxx.", "..xxxx..", ".xxxxxx.", ".xx..xx.", "x......x"}},
            {Icon::Bolt, {"....xxx.", "...xxx..", "..xxx...", ".xxxxxx.", "...xxx..", "..xxx...", ".xx.....", "x......."}},
            {Icon::Drop, {"...x....", "...x....", "..xxx...", "..xxx...", ".xxxxx..", ".xxxxx..", "..xxx...", "........"}},
            {Icon::Dust, {"........", "........", "..x.x...", ".xxxxx..", "xxxxxxx.", "xxxxxxxx", ".xxxxxx.", "........"}},
            {Icon::Moon, {"..xxx...", ".xx.....", "xx......", "xx......", "xx......", "xx.....x", ".xx..xx.", "..xxxx.."}},
            {Icon::Zzz, {"xxxxx...", "...x....", "..x.....", ".x......", "xxxxx...", "....xxx.", ".....x..", "....xxx."}},
        };
    } // namespace

    void drawIcon(gfx::Canvas& c, Icon icon, int16_t x, int16_t y, uint8_t scale, uint16_t color) {
        for (const auto& entry: kIcons) {
            if (entry.icon != icon) continue;
            for (int r = 0; r < 8; ++r)
                for (int k = 0; k < 8; ++k)
                    if (entry.rows[r][k] == 'x') c.fillRect(x + k * scale, y + r * scale, scale, scale, color);
            return;
        }
    }

    void drawDust(gfx::Canvas& c, int16_t cx, int16_t groundY, uint32_t ms, uint8_t dim) {
        // A dust bunny: a grey fluffball with two dot eyes that bobs a little.
        constexpr const char* kRows[] = {
            "..k.k.k..",
            ".kgggggk.",
            "kgggggggk",
            "kgegggegk",
            "kgggggggk",
            ".kkkkkkk.",
        };
        const uint8_t s = 3;
        const int16_t bob = ((ms / 400 + cx) % 2) ? 0 : 1;
        const int16_t left = cx - 9 * s / 2, top = groundY - 6 * s + bob;
        for (int r = 0; r < 6; ++r)
            for (int k = 0; k < 9; ++k) {
                const char p = kRows[r][k];
                if (p == '.') continue;
                uint16_t color = p == 'k' ? gfx::rgb(70, 66, 74) : p == 'e' ? gfx::rgb(20, 18, 24) : gfx::rgb(150, 146, 156);
                c.fillRect(left + k * s, top + r * s, s, s, darken(color, dim));
            }
    }

    void drawSparkle(gfx::Canvas& c, int16_t cx, int16_t cy, uint8_t size, uint16_t color) {
        c.fillRect(cx - size, cy, size * 2 + 1, 1, color);
        c.fillRect(cx, cy - size, 1, size * 2 + 1, color);
        if (size >= 3) {
            c.fillRect(cx - 1, cy - 1, 3, 3, color);
        }
    }

} // namespace art
