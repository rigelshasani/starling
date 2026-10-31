// Desktop simulator: drives the real game and UI code through scripted scenes and writes PNG
// screenshots at the device's 640x172 resolution. Build and run with: pio run -e sim -t exec
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <zlib.h>

#include "game/Pet.h"
#include "gfx/Canvas.h"
#include "ui/App.h"

namespace {

    class MemoryCanvas final : public gfx::Canvas {
    public:
        std::vector<uint16_t> pixels = std::vector<uint16_t>(gfx::kWidth * gfx::kHeight, 0);

        void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
            const int x0 = std::max<int>(0, x), y0 = std::max<int>(0, y);
            const int x1 = std::min<int>(gfx::kWidth, x + w), y1 = std::min<int>(gfx::kHeight, y + h);
            for (int yy = y0; yy < y1; ++yy)
                for (int xx = x0; xx < x1; ++xx) pixels[yy * gfx::kWidth + xx] = color;
        }

        bool writePng(const std::string& path, int zoom = 2) const {
            const int w = gfx::kWidth * zoom, h = gfx::kHeight * zoom;
            std::vector<uint8_t> raw;
            raw.reserve(static_cast<size_t>((w * 3 + 1) * h));
            for (int y = 0; y < h; ++y) {
                raw.push_back(0);
                for (int x = 0; x < w; ++x) {
                    const uint16_t p = pixels[(y / zoom) * gfx::kWidth + x / zoom];
                    raw.push_back(static_cast<uint8_t>(((p >> 11) & 0x1F) * 255 / 31));
                    raw.push_back(static_cast<uint8_t>(((p >> 5) & 0x3F) * 255 / 63));
                    raw.push_back(static_cast<uint8_t>((p & 0x1F) * 255 / 31));
                }
            }
            uLongf packedSize = compressBound(raw.size());
            std::vector<uint8_t> packed(packedSize);
            if (compress2(packed.data(), &packedSize, raw.data(), raw.size(), 9) != Z_OK) return false;
            packed.resize(packedSize);

            FILE* f = std::fopen(path.c_str(), "wb");
            if (!f) return false;
            const auto u32 = [](uint32_t v) { return std::vector<uint8_t>{uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; };
            const auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
                const auto len = u32(static_cast<uint32_t>(data.size()));
                std::fwrite(len.data(), 1, 4, f);
                std::vector<uint8_t> body(type, type + 4);
                body.insert(body.end(), data.begin(), data.end());
                std::fwrite(body.data(), 1, body.size(), f);
                const auto crc = u32(static_cast<uint32_t>(crc32(0, body.data(), static_cast<uInt>(body.size()))));
                std::fwrite(crc.data(), 1, 4, f);
            };
            const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
            std::fwrite(signature, 1, 8, f);
            std::vector<uint8_t> ihdr = u32(static_cast<uint32_t>(w));
            const auto hh = u32(static_cast<uint32_t>(h));
            ihdr.insert(ihdr.end(), hh.begin(), hh.end());
            ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
            chunk("IHDR", ihdr);
            chunk("IDAT", packed);
            chunk("IEND", {});
            std::fclose(f);
            return true;
        }
    };

    struct Sim {
        game::Pet pet;
        ui::App app{pet};
        MemoryCanvas canvas;
        uint32_t now = 1000;

        // Run the app for `ms` of real time at `scale`x game speed, with optional touch.
        void run(uint32_t ms, float scale = 1, const ui::Input& input = {}) {
            for (uint32_t t = 0; t < ms; t += 33) {
                now += 33;
                app.update(input, now, scale);
            }
        }
        void tap(int16_t x, int16_t y) {
            ui::Input down{true, x, y, false};
            app.update(down, now += 33, 1);
            app.update({}, now += 33, 1);
        }
        void drag(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int steps = 12) {
            for (int i = 0; i <= steps; ++i) {
                ui::Input in{true, static_cast<int16_t>(x0 + (x1 - x0) * i / steps), static_cast<int16_t>(y0 + (y1 - y0) * i / steps), false};
                app.update(in, now += 33, 1);
            }
            app.update({}, now += 33, 1);
        }
        void shot(const std::string& dir, const std::string& name) {
            app.render(canvas, now);
            const std::string path = dir + "/" + name + ".png";
            std::printf("%s %s\n", canvas.writePng(path) ? "wrote" : "FAILED", path.c_str());
        }
    };

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";

    Sim s;
    s.run(5000);
    s.shot(dir, "01-egg");
    s.run(80000);
    s.shot(dir, "02-egg-cracking");
    s.run(6000);
    s.run(400);
    s.shot(dir, "03-hatched");
    s.run(4000);
    s.shot(dir, "04-spark-home");
    s.tap(520, 30);
    s.shot(dir, "05-feed-menu");
    s.tap(150, 95);
    s.run(700);
    s.shot(dir, "06-eating");

    // Neglect for a while: dust, hunger and sickness.
    s.pet.dust = 3;
    s.pet.hunger = 15;
    s.pet.sick = true;
    s.run(1500);
    s.shot(dir, "07-needs-care");
    s.tap(520, 85);  // CLEAN
    s.run(300);
    s.shot(dir, "08-clean-mode");
    s.drag(20, 150, 470, 150, 30);
    s.run(300);
    s.shot(dir, "09-cleaned");

    s.tap(520, 140);  // HEAL
    s.run(600);
    s.shot(dir, "10-heal");

    s.pet.stage = game::Stage::Twinkle;
    s.pet.hunger = 70;
    s.pet.joy = 90;
    s.run(2000);
    s.shot(dir, "11-twinkle");
    s.pet.energy = 30;
    s.tap(600, 85);  // LIGHT
    s.run(2500);
    s.shot(dir, "12-night");
    s.tap(600, 85);
    s.pet.asleep = false;
    s.pet.energy = 90;

    s.tap(600, 30);  // PLAY
    s.run(4000, 1, ui::Input{true, 300, 140, false});
    s.run(3000, 1, ui::Input{true, 420, 140, false});
    s.shot(dir, "13-star-catch");
    s.run(15000, 1, ui::Input{true, 320, 140, false});
    s.app.update({}, s.now += 33, 1);
    s.shot(dir, "14-game-over");
    s.tap(320, 90);
    s.run(500);

    s.tap(600, 140);  // STATS
    s.shot(dir, "15-stats");
    s.tap(380, 140);
    s.shot(dir, "16-confirm");
    s.tap(320, 118);

    const game::Form forms[] = {game::Form::Nova, game::Form::Comet, game::Form::Nebula, game::Form::Dim};
    const char* names[] = {"17-nova", "18-comet", "19-nebula", "20-dim"};
    for (int i = 0; i < 4; ++i) {
        s.pet.stage = game::Stage::Adult;
        s.pet.form = forms[i];
        s.pet.hunger = 80;
        s.pet.joy = 85;
        s.run(1200);
        s.shot(dir, names[i]);
    }
    return 0;
}
