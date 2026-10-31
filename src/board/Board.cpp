#include "board/Board.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

#include <algorithm>
#include <iterator>

#ifndef STARLING_BOARD_REV
#define STARLING_BOARD_REV 2
#endif

namespace board {

    namespace {

        // ---- Pins (LCD 3.49). Rev1 and Rev2 differ in backlight, display reset and touch IRQ wiring.
        constexpr int kCs = 9, kSclk = 10, kD0 = 11, kD1 = 12, kD2 = 13, kD3 = 14;
#if STARLING_BOARD_REV == 1
        constexpr int kBacklight = 8;
        constexpr int kDisplayReset = 21;
#else
        constexpr int kBacklight = 42;
        constexpr int kDisplayReset = -1;  // through the IO expander
#endif
        constexpr int kBootPin = 0;
        constexpr int kPowerPin = 16;
        constexpr int kBatteryAdc = 4;
        constexpr int kTouchSda = 17, kTouchScl = 18, kSystemSda = 47, kSystemScl = 48;
        constexpr uint8_t kTouchAddress = 0x3B;
        constexpr uint16_t kPanelW = 172, kPanelH = 640;

        // TCA9554 IO expander on the system bus.
        constexpr uint8_t kExpander = 0x20;
        constexpr uint8_t kExBacklightEnable = 1, kExDisplayReset = 5, kExSysEnable = 6;

        Arduino_ESP32QSPI bus(kCs, kSclk, kD0, kD1, kD2, kD3, false);
        Arduino_AXS15231B panel(&bus, kDisplayReset, 0, false, kPanelW, kPanelH, 0, 0, 0, 0);
        Arduino_Canvas screen(kPanelW, kPanelH, &panel, 0, 0, 0);

        class ScreenCanvas final : public gfx::Canvas {
        public:
            void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
                if (w > 0 && h > 0) screen.fillRect(x, y, w, h, color);
            }
        } surface;

        bool expanderRead(uint8_t reg, uint8_t& value) {
            Wire1.beginTransmission(kExpander);
            Wire1.write(reg);
            if (Wire1.endTransmission(false) != 0 || Wire1.requestFrom(kExpander, static_cast<size_t>(1)) != 1) return false;
            value = Wire1.read();
            return true;
        }

        bool expanderWrite(uint8_t reg, uint8_t value) {
            Wire1.beginTransmission(kExpander);
            Wire1.write(reg);
            Wire1.write(value);
            return Wire1.endTransmission(true) == 0;
        }

        // Drive one expander pin as an output (registers: 0x01 output, 0x03 config with 1 = input).
        bool expanderOutput(uint8_t pin, bool high) {
            uint8_t output = 0xFF, config = 0xFF;
            if (!expanderRead(0x01, output)) return false;
            output = high ? (output | (1 << pin)) : (output & ~(1 << pin));
            if (!expanderWrite(0x01, output) || !expanderRead(0x03, config)) return false;
            return expanderWrite(0x03, config & ~(1 << pin));
        }

        uint8_t brightness = 70;

        void writeBacklight() {
            if (brightness == 0) {
                // The backlight circuit is inverted: a high level switches it off.
                pinMode(kBacklight, OUTPUT);
                digitalWrite(kBacklight, HIGH);
                return;
            }
            // Map 1-100% onto the usable duty range; the panel's PWM curve is dead below ~40%.
            constexpr uint8_t kMinDuty = 102;
            const uint8_t duty = static_cast<uint8_t>(kMinDuty + (255 - kMinDuty) * std::min<uint8_t>(brightness, 100) / 100);
            analogWriteResolution(kBacklight, 8);
            analogWriteFrequency(kBacklight, 25000);
            analogWrite(kBacklight, 255 - duty);
        }

    } // namespace

    bool begin() {
        pinMode(kBootPin, INPUT_PULLUP);
        pinMode(kPowerPin, INPUT_PULLUP);
        pinMode(kBacklight, OUTPUT);
        digitalWrite(kBacklight, HIGH);  // dark until the panel is ready

        Wire.begin(kTouchSda, kTouchScl);
        Wire.setClock(300000);
        Wire.setTimeOut(10);
        Wire1.begin(kSystemSda, kSystemScl);
        Wire1.setClock(300000);
        Wire1.setTimeOut(10);

        // Hold system power on so the board keeps running on battery after PWR is released.
        if (!expanderOutput(kExSysEnable, true)) log_w("power hold failed");
        expanderOutput(kExBacklightEnable, true);

#if STARLING_BOARD_REV != 1
        // Rev2 routes the LCD reset through the expander: high, low, high with Waveshare's timing.
        bool ok = expanderOutput(kExDisplayReset, true);
        if (ok) { delay(30); ok = expanderOutput(kExDisplayReset, false); }
        if (ok) { delay(250); ok = expanderOutput(kExDisplayReset, true); }
        if (!ok) log_e("display reset through expander failed");
        delay(30);
#endif

        analogReadResolution(12);
        analogRead(kBatteryAdc);  // claims the pin for the ADC before setting attenuation
        analogSetPinAttenuation(kBatteryAdc, ADC_11db);

        if (!screen.begin(40000000)) {
            log_e("display init failed");
            return false;
        }
        screen.setRotation(1);  // landscape, buttons on the left edge
        screen.fillScreen(0x0000);
        screen.flush();
        writeBacklight();
        return true;
    }

    gfx::Canvas& canvas() { return surface; }

    void flush() { screen.flush(); }

    void setBrightness(uint8_t percent) {
        if (percent == brightness) return;
        brightness = percent;
        writeBacklight();
    }

    bool readTouch(int16_t& x, int16_t& y) {
        static constexpr uint8_t kRead[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
        uint8_t data[8] = {};
        Wire.beginTransmission(kTouchAddress);
        Wire.write(kRead, sizeof kRead);
        if (Wire.endTransmission(false) != 0) return false;
        if (Wire.requestFrom(kTouchAddress, static_cast<size_t>(sizeof data), true) != sizeof data) return false;
        for (uint8_t& b: data) b = Wire.read();

        const uint8_t points = data[1];
        if (points == 0 || points > 4) return false;
        const uint16_t longAxis = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
        const uint16_t shortAxis = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);
        if (shortAxis > kPanelW || longAxis > kPanelH) return false;  // filler packet
        // Panel coordinates (portrait) -> landscape, matching setRotation(1).
        const uint16_t physX = std::min<uint16_t>(shortAxis, kPanelW - 1);
        const uint16_t physY = longAxis >= kPanelH ? 0 : static_cast<uint16_t>(kPanelH - 1 - longAxis);
        x = static_cast<int16_t>(physY);
        y = static_cast<int16_t>(kPanelW - 1 - physX);
        return true;
    }

    bool bootDown() { return digitalRead(kBootPin) == LOW; }
    bool powerDown() { return digitalRead(kPowerPin) == LOW; }

    float batteryVoltage() {
        uint32_t samples[16];
        for (auto& s: samples) {
            s = analogReadMilliVolts(kBatteryAdc);
            delayMicroseconds(300);
        }
        std::sort(std::begin(samples), std::end(samples));
        uint32_t total = 0;
        for (int i = 2; i < 14; ++i) total += samples[i];
        return static_cast<float>(total) / 12.0f * 3.0f / 1000.0f;  // 1:3 divider
    }

    int batteryPercent(float v) {
        if (v < 2.5f || v > 4.6f) return -1;
        constexpr float kVolts[] = {3.30f, 3.50f, 3.60f, 3.65f, 3.70f, 3.75f, 3.79f, 3.85f, 3.92f, 4.00f, 4.10f, 4.15f};
        constexpr int kPercent[] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        if (v <= kVolts[0]) return 0;
        for (int i = 1; i < 12; ++i)
            if (v < kVolts[i])
                return static_cast<int>(kPercent[i - 1] + (kPercent[i] - kPercent[i - 1]) * (v - kVolts[i - 1]) / (kVolts[i] - kVolts[i - 1]));
        return 100;
    }

    void powerOff() {
        setBrightness(0);
        expanderOutput(kExBacklightEnable, false);
        expanderOutput(kExSysEnable, false);
    }

} // namespace board
