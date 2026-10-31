#include "game/Pet.h"

#include <algorithm>

namespace game {

    namespace {

        float clamp100(float v) { return std::clamp(v, 0.0f, 100.0f); }

        uint32_t next(Pet& pet) {
            // xorshift32: deterministic per pet, persisted with the save.
            uint32_t x = pet.rng ? pet.rng : 0x5EED1234;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            return pet.rng = x;
        }

        float uniform(Pet& pet, float lo, float hi) {
            return lo + (hi - lo) * static_cast<float>(next(pet) % 10000) / 10000.0f;
        }

        bool chancePerMinute(Pet& pet, float percent, float seconds) {
            return uniform(pet, 0, 100) < percent * seconds / 60.0f;
        }

        void fallAsleep(Pet& pet) {
            pet.asleep = true;
            pet.lightsOnAsleepSeconds = 0;
        }

        Form chooseForm(const Pet& pet) {
            const float avgJoy = pet.stageSeconds > 0 ? pet.joyTime / pet.stageSeconds : 50;
            const float sleepShare = pet.stageSeconds > 0 ? pet.sleepSeconds / pet.stageSeconds : 0;
            if (pet.careMistakes >= 6) return Form::Dim;
            if (pet.careMistakes <= 2 && avgJoy >= 60) return Form::Nova;
            if (pet.plays >= 6 && pet.plays * 2 >= pet.meals) return Form::Comet;
            if (sleepShare >= 0.35f) return Form::Nebula;
            return pet.careMistakes <= 3 ? Form::Nova : Form::Nebula;
        }

        void enterStage(Pet& pet, Stage stage) {
            pet.stage = stage;
            pet.stageSeconds = 0;
            pet.sleepSeconds = 0;
            pet.joyTime = 0;
        }

    } // namespace

    void newEgg(Pet& pet, uint32_t seed) {
        const uint32_t generation = pet.generation + 1;
        pet = Pet{};
        pet.generation = generation;
        pet.rng = seed ? seed : 0x5EED1234;
    }

    Reaction tick(Pet& pet, float s) {
        if (s <= 0) return Reaction::Ok;
        pet.ageSeconds += s;
        pet.stageSeconds += s;

        if (pet.stage == Stage::Egg) {
            if (pet.stageSeconds >= tuning::kHatchSeconds) {
                enterStage(pet, Stage::Spark);
                return Reaction::Hatched;
            }
            return Reaction::Ok;
        }

        const float h = s / 3600.0f;
        if (pet.asleep) {
            pet.sleepSeconds += s;
            pet.hunger = clamp100(pet.hunger - 4 * h);
            pet.joy = clamp100(pet.joy - 1 * h);
            pet.energy = clamp100(pet.energy + (pet.lightsOff ? 40 : 14) * h);
            if (!pet.lightsOff) {
                pet.lightsOnAsleepSeconds += s;
                if (pet.lightsOnAsleepSeconds >= 5 * 60 && pet.lightsOnAsleepSeconds - s < 5 * 60) ++pet.careMistakes;
            }
            if (pet.energy >= 100) pet.asleep = false;
        } else {
            pet.hunger = clamp100(pet.hunger - 14 * h);
            pet.joy = clamp100(pet.joy - 10 * h);
            pet.energy = clamp100(pet.energy - 9 * h);
            if (pet.energy < 15) fallAsleep(pet);
            pet.nextDustIn -= s;
            if (pet.nextDustIn <= 0) {
                pet.dust = std::min<uint8_t>(4, pet.dust + 1);
                pet.nextDustIn = uniform(pet, 1800, 4200);
            }
        }

        // Health: sickness and neglect wear it down; good care slowly restores it.
        float healthRate = 4;
        if (pet.sick) healthRate = -10;
        else if (pet.hunger <= 0) healthRate = -8;
        else if (pet.dust >= 3) healthRate = -3;
        pet.health = clamp100(pet.health + healthRate * h);
        if (!pet.sick) {
            float risk = 0;
            if (pet.dust >= 3) risk += 1;
            if (pet.hunger < 10) risk += 1;
            if (pet.weight > 40) risk += 0.5f;
            if ((risk > 0 && chancePerMinute(pet, risk, s)) || pet.health < 30) pet.sick = true;
        }

        // Care mistakes: needs left unanswered for too long.
        if (needsAttention(pet)) {
            pet.neglectSeconds += s;
            if (pet.neglectSeconds >= tuning::kNeglectSeconds) {
                ++pet.careMistakes;
                pet.neglectSeconds = 0;
            }
        } else {
            pet.neglectSeconds = 0;
        }

        pet.joyTime += pet.joy * s;
        if (pet.stage == Stage::Spark && pet.stageSeconds >= tuning::kSparkSeconds) {
            enterStage(pet, Stage::Twinkle);
            return Reaction::Evolved;
        }
        if (pet.stage == Stage::Twinkle && pet.stageSeconds >= tuning::kTwinkleSeconds) {
            pet.form = chooseForm(pet);
            enterStage(pet, Stage::Adult);
            return Reaction::Evolved;
        }
        return Reaction::Ok;
    }

    Reaction feedMeal(Pet& pet) {
        if (pet.stage == Stage::Egg) return Reaction::Refused;
        if (pet.asleep) return Reaction::Asleep;
        if (pet.hunger >= 95) return Reaction::Full;
        pet.hunger = clamp100(pet.hunger + 30);
        pet.weight += 1;
        ++pet.meals;
        pet.nextDustIn = std::min(pet.nextDustIn, 900.0f);
        return Reaction::Ok;
    }

    Reaction feedSnack(Pet& pet) {
        if (pet.stage == Stage::Egg) return Reaction::Refused;
        if (pet.asleep) return Reaction::Asleep;
        pet.hunger = clamp100(pet.hunger + 8);
        pet.joy = clamp100(pet.joy + 15);
        pet.weight += 2;
        ++pet.snacks;
        return Reaction::Ok;
    }

    Reaction play(Pet& pet, int score) {
        if (pet.stage == Stage::Egg) return Reaction::Refused;
        if (pet.asleep) return Reaction::Asleep;
        pet.joy = clamp100(pet.joy + std::min(30, 6 + score * 2));
        pet.energy = clamp100(pet.energy - 8);
        pet.hunger = clamp100(pet.hunger - 4);
        pet.weight = std::max(5.0f, pet.weight - 1);
        ++pet.plays;
        return Reaction::Ok;
    }

    Reaction clean(Pet& pet, uint8_t piles) {
        if (piles == 0 || pet.dust == 0) return Reaction::Refused;
        pet.dust = static_cast<uint8_t>(pet.dust - std::min(piles, pet.dust));
        pet.joy = clamp100(pet.joy + 2);
        ++pet.cleans;
        return Reaction::Ok;
    }

    Reaction toggleLights(Pet& pet) {
        pet.lightsOff = !pet.lightsOff;
        if (pet.lightsOff && !pet.asleep && pet.stage != Stage::Egg && pet.energy < 60) fallAsleep(pet);
        return Reaction::Ok;
    }

    Reaction medicine(Pet& pet) {
        if (!pet.sick) {
            pet.joy = clamp100(pet.joy - 5);
            return Reaction::NotSick;
        }
        pet.sick = false;
        pet.health = std::max(pet.health, 60.0f);
        return Reaction::Ok;
    }

    Reaction stroke(Pet& pet) {
        if (pet.stage == Stage::Egg) return Reaction::Ok;
        if (pet.asleep) {
            pet.asleep = false;
            pet.joy = clamp100(pet.joy - 3);
            return Reaction::Grumpy;
        }
        pet.joy = clamp100(pet.joy + 3);
        ++pet.pets;
        return Reaction::Ok;
    }

    Mood mood(const Pet& pet) {
        if (pet.asleep) return Mood::Asleep;
        if (pet.sick) return Mood::Sick;
        if (pet.hunger < tuning::kNeed) return Mood::Hungry;
        if (pet.energy < 20) return Mood::Sleepy;
        if (pet.dust >= 2) return Mood::Dusty;
        if (pet.joy < tuning::kNeed) return Mood::Bored;
        if (pet.joy > 70 && pet.hunger > 50) return Mood::Happy;
        return Mood::Content;
    }

    bool needsAttention(const Pet& pet) {
        if (pet.stage == Stage::Egg) return false;
        if (pet.asleep) return !pet.lightsOff;
        return pet.sick || pet.hunger < tuning::kNeed || pet.joy < tuning::kNeed || pet.dust >= 2 || pet.energy < 20;
    }

    const char* stageName(const Pet& pet) {
        switch (pet.stage) {
        case Stage::Egg: return "Egg";
        case Stage::Spark: return "Spark";
        case Stage::Twinkle: return "Twinkle";
        case Stage::Adult:
            switch (pet.form) {
            case Form::Nova: return "Nova";
            case Form::Comet: return "Comet";
            case Form::Nebula: return "Nebula";
            case Form::Dim: return "Dim";
            default: return "Star";
            }
        }
        return "?";
    }

    const char* moodName(Mood mood) {
        switch (mood) {
        case Mood::Happy: return "happy";
        case Mood::Content: return "content";
        case Mood::Hungry: return "hungry";
        case Mood::Sleepy: return "sleepy";
        case Mood::Bored: return "bored";
        case Mood::Dusty: return "dusty";
        case Mood::Sick: return "sick";
        case Mood::Asleep: return "asleep";
        }
        return "";
    }

} // namespace game
