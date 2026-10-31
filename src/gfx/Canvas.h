// Minimal drawing surface shared by the device (Arduino_GFX) and the desktop simulator.
// Everything the game draws goes through fillRect, so both targets render identically.
#pragma once

#include <cstdint>
#include <string_view>

namespace gfx {

    constexpr int16_t kWidth = 640;
    constexpr int16_t kHeight = 172;

    constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    // Linear blend between two RGB565 colors; t in 0..255.
    uint16_t mix(uint16_t a, uint16_t b, uint8_t t);

    class Canvas {
    public:
        virtual ~Canvas() = default;
        virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;

        void fill(uint16_t color) { fillRect(0, 0, kWidth, kHeight, color); }
        void pixel(int16_t x, int16_t y, uint16_t color) { fillRect(x, y, 1, 1, color); }
        void hline(int16_t x, int16_t y, int16_t w, uint16_t color) { fillRect(x, y, w, 1, color); }
        void vline(int16_t x, int16_t y, int16_t h, uint16_t color) { fillRect(x, y, 1, h, color); }
        void frame(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, int16_t t = 1);
        // Rounded-corner box with chunky (pixel-art) corners of `r` pixels.
        void roundBox(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
        void disc(int16_t cx, int16_t cy, int16_t r, uint16_t color);

        // 5x7 pixel font. Returns the drawn width.
        int16_t text(int16_t x, int16_t y, std::string_view s, uint8_t scale, uint16_t color);
        static int16_t textWidth(std::string_view s, uint8_t scale);
        void textCentered(int16_t cx, int16_t y, std::string_view s, uint8_t scale, uint16_t color);
    };

} // namespace gfx
