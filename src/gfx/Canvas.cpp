#include "gfx/Canvas.h"

#include "gfx/Font5x7.h"

namespace gfx {

    uint16_t mix(uint16_t a, uint16_t b, uint8_t t) {
        const auto channel = [t](int ca, int cb) { return ca + ((cb - ca) * t) / 255; };
        const int r = channel(a >> 11, b >> 11);
        const int g = channel((a >> 5) & 0x3F, (b >> 5) & 0x3F);
        const int bl = channel(a & 0x1F, b & 0x1F);
        return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
    }

    void Canvas::frame(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, int16_t t) {
        fillRect(x, y, w, t, color);
        fillRect(x, y + h - t, w, t, color);
        fillRect(x, y + t, t, h - 2 * t, color);
        fillRect(x + w - t, y + t, t, h - 2 * t, color);
    }

    void Canvas::roundBox(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
        fillRect(x + r, y, w - 2 * r, h, color);
        for (int16_t i = 0; i < r; ++i) {
            // Stepped corners read as pixel art at any size.
            const int16_t inset = r - i;
            fillRect(x + i, y + inset, 1, h - 2 * inset, color);
            fillRect(x + w - 1 - i, y + inset, 1, h - 2 * inset, color);
        }
    }

    void Canvas::disc(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
        for (int16_t dy = -r; dy <= r; ++dy) {
            int16_t dx = 0;
            while ((dx + 1) * (dx + 1) + dy * dy <= r * r) ++dx;
            fillRect(cx - dx, cy + dy, 2 * dx + 1, 1, color);
        }
    }

    int16_t Canvas::text(int16_t x, int16_t y, std::string_view s, uint8_t scale, uint16_t color) {
        int16_t cx = x;
        for (char ch: s) {
            if (ch < font5x7::kFirst || ch > font5x7::kLast) ch = '?';
            const uint8_t* glyph = font5x7::kGlyphs[ch - font5x7::kFirst];
            for (int16_t col = 0; col < 5; ++col) {
                const uint8_t bits = glyph[col];
                int16_t row = 0;
                while (row < 7) {
                    if (!(bits & (1 << row))) { ++row; continue; }
                    int16_t run = row;
                    while (run < 7 && (bits & (1 << run))) ++run;
                    fillRect(cx + col * scale, y + row * scale, scale, (run - row) * scale, color);
                    row = run;
                }
            }
            cx += 6 * scale;
        }
        return cx - x;
    }

    int16_t Canvas::textWidth(std::string_view s, uint8_t scale) {
        return s.empty() ? 0 : static_cast<int16_t>(s.size() * 6 * scale - scale);
    }

    void Canvas::textCentered(int16_t cx, int16_t y, std::string_view s, uint8_t scale, uint16_t color) {
        text(cx - textWidth(s, scale) / 2, y, s, scale, color);
    }

} // namespace gfx
