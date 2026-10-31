// Game-rule tests: run on this computer with `pio test -e native`.
#include <unity.h>

#include "game/Pet.h"

using namespace game;

namespace {
    // Advance in 1-second steps like the device loop (only coarser).
    Reaction advance(Pet& pet, float seconds) {
        Reaction last = Reaction::Ok;
        for (float t = 0; t < seconds; t += 1) {
            const Reaction r = tick(pet, 1);
            if (r != Reaction::Ok) last = r;
        }
        return last;
    }

    Pet hatched() {
        Pet pet;
        advance(pet, tuning::kHatchSeconds + 1);
        return pet;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_egg_hatches_and_needs_nothing() {
    Pet pet;
    advance(pet, 30);
    TEST_ASSERT_EQUAL(Stage::Egg, pet.stage);
    TEST_ASSERT_EQUAL_FLOAT(80, pet.hunger);  // eggs don't get hungry
    TEST_ASSERT_FALSE(needsAttention(pet));
    TEST_ASSERT_EQUAL(Reaction::Refused, feedMeal(pet));
    TEST_ASSERT_EQUAL(Reaction::Hatched, advance(pet, tuning::kHatchSeconds));
    TEST_ASSERT_EQUAL(Stage::Spark, pet.stage);
}

void test_needs_drift_and_feeding_helps() {
    Pet pet = hatched();
    const float before = pet.hunger;
    advance(pet, 3600);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, before - 14, pet.hunger);
    TEST_ASSERT_EQUAL(Reaction::Ok, feedMeal(pet));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, before + 16, pet.hunger);
    pet.hunger = 99;
    TEST_ASSERT_EQUAL(Reaction::Full, feedMeal(pet));
}

void test_ignored_needs_become_care_mistakes() {
    Pet pet = hatched();
    pet.hunger = 5;
    advance(pet, tuning::kNeglectSeconds + 5);
    TEST_ASSERT_EQUAL(1, pet.careMistakes);
    feedMeal(pet);
    feedMeal(pet);
    pet.joy = 90;
    pet.neglectSeconds = 0;
    advance(pet, tuning::kNeglectSeconds + 5);
    TEST_ASSERT_EQUAL(1, pet.careMistakes);  // answered in time: no new mistake
}

void test_tired_pet_sleeps_and_wakes_rested() {
    Pet pet = hatched();
    pet.energy = 14;
    advance(pet, 1);
    TEST_ASSERT_TRUE(pet.asleep);
    TEST_ASSERT_EQUAL(Reaction::Asleep, feedMeal(pet));
    toggleLights(pet);
    TEST_ASSERT_TRUE(pet.lightsOff);
    advance(pet, 3 * 3600);
    TEST_ASSERT_FALSE(pet.asleep);
    TEST_ASSERT_EQUAL(0, pet.careMistakes);  // lights were off: good care
}

void test_lights_left_on_while_asleep_is_a_mistake() {
    Pet pet = hatched();
    pet.energy = 14;
    pet.joy = 90;
    pet.hunger = 90;
    advance(pet, 6 * 60);
    TEST_ASSERT_EQUAL(1, pet.careMistakes);
}

void test_medicine_cures_only_when_sick() {
    Pet pet = hatched();
    TEST_ASSERT_EQUAL(Reaction::NotSick, medicine(pet));
    pet.sick = true;
    pet.health = 20;
    TEST_ASSERT_EQUAL(Reaction::Ok, medicine(pet));
    TEST_ASSERT_FALSE(pet.sick);
    TEST_ASSERT_EQUAL_FLOAT(60, pet.health);
}

void test_dust_piles_up_and_cleaning_clears_it() {
    Pet pet = hatched();
    pet.nextDustIn = 1;
    advance(pet, 2);
    TEST_ASSERT_EQUAL(1, pet.dust);
    TEST_ASSERT_EQUAL(Reaction::Ok, clean(pet, 3));
    TEST_ASSERT_EQUAL(0, pet.dust);
    TEST_ASSERT_EQUAL(Reaction::Refused, clean(pet, 1));
}

void test_good_care_grows_a_nova_and_neglect_a_dim() {
    Pet good;
    good.stage = Stage::Twinkle;
    good.joy = 90;
    good.hunger = 90;
    good.energy = 90;
    good.nextDustIn = 1e9f;
    good.stageSeconds = tuning::kTwinkleSeconds - 2;
    good.joyTime = 90 * good.stageSeconds;
    TEST_ASSERT_EQUAL(Reaction::Evolved, advance(good, 3));
    TEST_ASSERT_EQUAL(Stage::Adult, good.stage);
    TEST_ASSERT_EQUAL(Form::Nova, good.form);

    Pet neglected = good;
    neglected.stage = Stage::Twinkle;
    neglected.form = Form::None;
    neglected.careMistakes = 7;
    neglected.stageSeconds = tuning::kTwinkleSeconds - 2;
    advance(neglected, 3);
    TEST_ASSERT_EQUAL(Form::Dim, neglected.form);

    Pet playful = good;
    playful.stage = Stage::Twinkle;
    playful.careMistakes = 4;
    playful.plays = 10;
    playful.meals = 8;
    playful.stageSeconds = tuning::kTwinkleSeconds - 2;
    advance(playful, 3);
    TEST_ASSERT_EQUAL(Form::Comet, playful.form);
}

void test_new_egg_counts_generations() {
    Pet pet = hatched();
    advance(pet, 100);
    newEgg(pet, 42);
    TEST_ASSERT_EQUAL(Stage::Egg, pet.stage);
    TEST_ASSERT_EQUAL(2, pet.generation);
    TEST_ASSERT_EQUAL_FLOAT(0, pet.ageSeconds);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_egg_hatches_and_needs_nothing);
    RUN_TEST(test_needs_drift_and_feeding_helps);
    RUN_TEST(test_ignored_needs_become_care_mistakes);
    RUN_TEST(test_tired_pet_sleeps_and_wakes_rested);
    RUN_TEST(test_lights_left_on_while_asleep_is_a_mistake);
    RUN_TEST(test_medicine_cures_only_when_sick);
    RUN_TEST(test_dust_piles_up_and_cleaning_clears_it);
    RUN_TEST(test_good_care_grows_a_nova_and_neglect_a_dim);
    RUN_TEST(test_new_egg_counts_generations);
    return UNITY_END();
}
