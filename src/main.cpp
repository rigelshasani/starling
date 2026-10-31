// Starling firmware entry point: input, frame loop, saving, dimming and power.
//
// Buttons: BOOT = stats / close. PWR short = screen off/on, PWR hold (2 s) = save and power off.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>

#include "board/Board.h"
#include "game/Pet.h"
#include "ui/App.h"

namespace {

    constexpr uint32_t kFrameMs = 33;
    constexpr uint32_t kDimAfterMs = 60 * 1000;
    constexpr uint32_t kSleepAfterMs = 3 * 60 * 1000;
    constexpr uint32_t kSaveEveryMs = 60 * 1000;
    constexpr uint32_t kMinSaveGapMs = 5 * 1000;
    constexpr uint32_t kPowerHoldMs = 2000;
    constexpr uint8_t kBright = 70, kDim = 15;

    game::Pet pet;
    ui::App app(pet);
    Preferences prefs;

    uint32_t lastSaveMs = 0;
    uint32_t lastFrameMs = 0;
    uint32_t lastBatteryMs = 0;
    bool screenOn = true;
    bool lowBatteryWarned = false;
    bool savePending = false;

    struct Button {
        bool down = false;
        uint32_t since = 0;
        bool longFired = false;
    } boot, power;

    void loadPet() {
        prefs.begin("starling", false);
        game::Pet saved;
        const size_t size = prefs.isKey("pet") ? prefs.getBytesLength("pet") : 0;
        if (size == sizeof saved && prefs.getBytes("pet", &saved, sizeof saved) == sizeof saved && saved.version == pet.version) {
            pet = saved;
            log_i("loaded %s, age %.0f s", game::stageName(pet), pet.ageSeconds);
        } else {
            pet = game::Pet{};
            pet.rng = esp_random() | 1;
            log_i("new egg");
            prefs.putBytes("pet", &pet, sizeof pet);
        }
    }

    void savePet(uint32_t now) {
        prefs.putBytes("pet", &pet, sizeof pet);
        lastSaveMs = now;
    }

    void setScreen(bool on, uint32_t now) {
        screenOn = on;
        board::setBrightness(on ? kBright : 0);
        if (on) app.noteActivity(now);
    }

    // Returns true on a short press release.
    bool shortPress(Button& b, bool down, uint32_t now) {
        if (down && !b.down) { b.down = true; b.since = now; b.longFired = false; return false; }
        if (!down && b.down) { b.down = false; return !b.longFired && now - b.since < 800; }
        return false;
    }

    void powerOffNow(uint32_t now) {
        savePet(now);
        prefs.end();
        board::powerOff();
        // Still running means USB is powering the board: wait for PWR, then restart.
        delay(800);
        while (!board::powerDown()) delay(50);
        ESP.restart();
    }

} // namespace

void setup() {
    Serial.begin(115200);
    if (!board::begin()) log_e("board init failed");
    loadPet();
    lastSaveMs = millis();
}

void loop() {
    const uint32_t now = millis();
    if (now - lastFrameMs < kFrameMs) {
        delay(2);
        return;
    }
    lastFrameMs = now;

    ui::Input input;
    int16_t x = 0, y = 0;
    const bool touching = board::readTouch(x, y);

    const bool bootShort = shortPress(boot, board::bootDown(), now);
    const bool powerDownNow = board::powerDown();
    const bool powerShort = shortPress(power, powerDownNow, now);
    if (power.down && !power.longFired && now - power.since >= kPowerHoldMs) {
        power.longFired = true;
        powerOffNow(now);
    }

    if (powerShort) setScreen(!screenOn, now);

    if (!screenOn) {
        // While the screen is off, the first touch or BOOT press only wakes it.
        if (touching || bootShort) {
            setScreen(true, now);
            while (board::readTouch(x, y)) delay(20);
        }
        app.update({}, now);
    } else {
        if (app.idleMs(now) > kSleepAfterMs) {
            setScreen(false, now);
        } else {
            board::setBrightness(app.idleMs(now) > kDimAfterMs ? kDim : kBright);
            input.touching = touching;
            input.x = x;
            input.y = y;
            input.bootPressed = bootShort;
            app.update(input, now);
            app.render(board::canvas(), now);
            board::flush();
        }
    }

    if (now - lastBatteryMs > 30000) {
        lastBatteryMs = now;
        const int percent = board::batteryPercent(board::batteryVoltage());
        if (percent >= 0 && percent <= 10 && !lowBatteryWarned) {
            lowBatteryWarned = true;
            app.toast("Battery low - charge me!", now, 5000);
        }
        if (percent > 20) lowBatteryWarned = false;
    }

    savePending |= app.takeSaveRequest();
    if ((savePending && now - lastSaveMs > kMinSaveGapMs) || now - lastSaveMs > kSaveEveryMs) {
        savePet(now);
        savePending = false;
    }
}
