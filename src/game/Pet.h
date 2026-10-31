// The starling's life: needs that drift over time, care actions, growth and evolution.
// Pure C++ with no hardware dependencies, so it runs in native tests and the simulator.
#pragma once

#include <cstdint>

namespace game {

    enum class Stage : uint8_t { Egg, Spark, Twinkle, Adult };
    enum class Form : uint8_t { None, Nova, Comet, Nebula, Dim };
    enum class Mood : uint8_t { Happy, Content, Hungry, Sleepy, Bored, Dusty, Sick, Asleep };

    // What an action did, so the UI can animate the right reaction.
    enum class Reaction : uint8_t { Ok, Refused, Full, NotSick, Asleep, Grumpy, Evolved, Hatched };

    // Tunables: real-time pacing, chosen so a pet grows up over about a day of on-time.
    namespace tuning {
        constexpr float kHatchSeconds = 90;
        constexpr float kSparkSeconds = 40 * 60;     // Spark -> Twinkle
        constexpr float kTwinkleSeconds = 8 * 3600;  // Twinkle -> adult
        constexpr float kNeglectSeconds = 15 * 60;   // unanswered need -> care mistake
        constexpr float kNeed = 25;                  // a stat under this asks for attention
    }

    struct Pet {
        uint16_t version = 1;
        Stage stage = Stage::Egg;
        Form form = Form::None;
        bool asleep = false;
        bool lightsOff = false;
        bool sick = false;
        uint8_t dust = 0;          // dust piles on the ground, 0..4
        float hunger = 80;         // 100 = full
        float joy = 80;
        float energy = 90;
        float health = 100;
        float weight = 10;
        float ageSeconds = 0;      // time the device has been on with this pet
        float stageSeconds = 0;
        float nextDustIn = 1800;
        float neglectSeconds = 0;
        float lightsOnAsleepSeconds = 0;
        float sleepSeconds = 0;    // during the current stage
        float joyTime = 0;         // integral of joy over the current stage
        uint16_t careMistakes = 0;
        uint16_t meals = 0, snacks = 0, plays = 0, cleans = 0, pets = 0;
        uint32_t generation = 1;
        uint32_t rng = 0x5EED1234;
    };

    void newEgg(Pet& pet, uint32_t seed);

    // Advance time. Returns Hatched/Evolved when the stage changed, else Ok.
    Reaction tick(Pet& pet, float seconds);

    Reaction feedMeal(Pet& pet);
    Reaction feedSnack(Pet& pet);
    Reaction play(Pet& pet, int score);
    Reaction clean(Pet& pet, uint8_t piles);
    Reaction toggleLights(Pet& pet);
    Reaction medicine(Pet& pet);
    Reaction stroke(Pet& pet);  // petting by touch

    Mood mood(const Pet& pet);
    bool needsAttention(const Pet& pet);
    const char* stageName(const Pet& pet);
    const char* moodName(Mood mood);

} // namespace game
