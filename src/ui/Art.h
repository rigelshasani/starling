// Pixel art: the starling at each stage, its faces and accessories, and the menu icons.
#pragma once

#include <cstdint>

#include "game/Pet.h"
#include "gfx/Canvas.h"

namespace art {

    enum class Eyes : uint8_t { Open, Blink, Happy, Closed, Sad, Dizzy };
    enum class Mouth : uint8_t { Smile, Open, Flat, Frown, Chew };

    struct Face {
        Eyes eyes = Eyes::Open;
        Mouth mouth = Mouth::Smile;
        bool blush = false;
    };

    struct Look {
        int16_t x = 0;          // bottom-centre of the sprite, screen pixels
        int16_t y = 0;
        uint8_t scale = 5;      // screen pixels per art pixel
        int8_t squash = 0;      // art pixels: >0 squat, <0 stretch (for hops)
        uint8_t dim = 0;        // 0..255 darkening (lights off)
        bool flip = false;      // mirror horizontally (walking left)
        uint8_t crack = 0;      // egg cracks, 0..3
    };

    // Draw the pet for its stage/form. Returns the sprite's height in screen pixels.
    int16_t drawPet(gfx::Canvas& c, const game::Pet& pet, const Look& look, const Face& face, uint32_t ms);
    int16_t petWidth(const game::Pet& pet, uint8_t scale);
    int16_t petHeight(const game::Pet& pet, uint8_t scale);

    enum class Icon : uint8_t { Feed, Play, Clean, Light, Heal, Stats, Heart, Star, Bolt, Drop, Dust, Moon, Zzz };
    void drawIcon(gfx::Canvas& c, Icon icon, int16_t x, int16_t y, uint8_t scale, uint16_t color);

    void drawDust(gfx::Canvas& c, int16_t cx, int16_t groundY, uint32_t ms, uint8_t dim);
    void drawSparkle(gfx::Canvas& c, int16_t cx, int16_t cy, uint8_t size, uint16_t color);

    namespace palette {
        constexpr uint16_t kSkyTop = gfx::rgb(11, 16, 38);
        constexpr uint16_t kSkyBottom = gfx::rgb(31, 38, 82);
        constexpr uint16_t kGround = gfx::rgb(58, 47, 79);
        constexpr uint16_t kGroundLight = gfx::rgb(82, 68, 108);
        constexpr uint16_t kGroundDark = gfx::rgb(40, 32, 56);
        constexpr uint16_t kPanel = gfx::rgb(21, 20, 19);
        constexpr uint16_t kButton = gfx::rgb(39, 34, 30);
        constexpr uint16_t kButtonHot = gfx::rgb(66, 59, 53);
        constexpr uint16_t kText = gfx::rgb(247, 244, 241);
        constexpr uint16_t kMuted = gfx::rgb(177, 167, 159);
        constexpr uint16_t kAccent = gfx::rgb(253, 111, 66);
        constexpr uint16_t kGold = gfx::rgb(255, 205, 80);
        constexpr uint16_t kPink = gfx::rgb(255, 120, 150);
        constexpr uint16_t kGreen = gfx::rgb(120, 220, 140);
        constexpr uint16_t kBlue = gfx::rgb(120, 190, 255);
        constexpr uint16_t kBad = gfx::rgb(240, 90, 90);
    }

} // namespace art
