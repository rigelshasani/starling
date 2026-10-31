// Waveshare ESP32-S3-Touch-LCD-3.49 hardware: display, backlight, touch, buttons, battery, power.
// Pin map and bring-up sequence adapted from RSVP Nano (MIT, https://github.com/ionutdecebal/rsvpnano).
#pragma once

#include <cstdint>

#include "gfx/Canvas.h"

namespace board {

    bool begin();

    // Landscape 640x172 drawing surface backed by a full framebuffer; call flush() once per frame.
    gfx::Canvas& canvas();
    void flush();

    void setBrightness(uint8_t percent);  // 0 turns the backlight off

    // Touch in landscape coordinates. Returns false when nothing is touching the screen.
    bool readTouch(int16_t& x, int16_t& y);

    bool bootDown();   // BOOT button (left side)
    bool powerDown();  // PWR button

    // Battery voltage and charge estimate; percent is -1 when no battery reads plausibly.
    float batteryVoltage();
    int batteryPercent(float voltage);

    // Release the battery power latch. On USB power the board stays on.
    void powerOff();

} // namespace board
