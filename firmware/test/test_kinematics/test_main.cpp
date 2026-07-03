// Host-run tests for the mecanum mixing math: `pio test -e native`
//
// These tests ARE the spec for mecanum::mix(). They pin down the sign
// conventions (vx+ forward, vy+ strafe right, omega+ clockwise) and the
// scaling (full-scale pure motion → ±255), but deliberately do NOT choose
// between clamp-vs-normalize overflow handling — both pass.

#include <unity.h>
#include "kinematics.h"

using mecanum::mix;
using mecanum::WheelSpeeds;
using mecanum::kMaxPwm;

static void assert_in_range(const WheelSpeeds& w) {
    TEST_ASSERT_TRUE(w.fl >= -kMaxPwm && w.fl <= kMaxPwm);
    TEST_ASSERT_TRUE(w.fr >= -kMaxPwm && w.fr <= kMaxPwm);
    TEST_ASSERT_TRUE(w.rl >= -kMaxPwm && w.rl <= kMaxPwm);
    TEST_ASSERT_TRUE(w.rr >= -kMaxPwm && w.rr <= kMaxPwm);
}

void test_zero_input_is_all_stop() {
    WheelSpeeds w = mix(0, 0, 0);
    TEST_ASSERT_EQUAL_INT16(0, w.fl);
    TEST_ASSERT_EQUAL_INT16(0, w.fr);
    TEST_ASSERT_EQUAL_INT16(0, w.rl);
    TEST_ASSERT_EQUAL_INT16(0, w.rr);
}

void test_full_forward_all_wheels_full_ahead() {
    WheelSpeeds w = mix(100, 0, 0);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.fl);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.fr);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.rl);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.rr);
}

void test_full_reverse_all_wheels_full_astern() {
    WheelSpeeds w = mix(-100, 0, 0);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.fl);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.fr);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.rl);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.rr);
}

void test_half_forward_is_proportional() {
    WheelSpeeds w = mix(50, 0, 0);
    TEST_ASSERT_INT_WITHIN(2, kMaxPwm / 2, w.fl);
    TEST_ASSERT_INT_WITHIN(2, kMaxPwm / 2, w.fr);
    TEST_ASSERT_INT_WITHIN(2, kMaxPwm / 2, w.rl);
    TEST_ASSERT_INT_WITHIN(2, kMaxPwm / 2, w.rr);
}

// Strafe right: FL and RR drive forward, FR and RL drive backward.
void test_strafe_right_diagonal_pairs() {
    WheelSpeeds w = mix(0, 100, 0);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.fl);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.fr);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.rl);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.rr);
}

void test_strafe_left_diagonal_pairs() {
    WheelSpeeds w = mix(0, -100, 0);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.fl);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.fr);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.rl);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.rr);
}

// Clockwise: left side forward, right side backward.
void test_rotate_clockwise_sides_oppose() {
    WheelSpeeds w = mix(0, 0, 100);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.fl);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.rl);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.fr);
    TEST_ASSERT_EQUAL_INT16(-kMaxPwm, w.rr);
}

// Unsaturated combined motion must keep exact mixing ratios (±2 for rounding).
// mix(40, 20, 10): raw FL=70 FR=10 RL=30 RR=50, scaled by 2.55.
void test_combined_motion_ratios() {
    WheelSpeeds w = mix(40, 20, 10);
    TEST_ASSERT_INT_WITHIN(2, 178, w.fl);
    TEST_ASSERT_INT_WITHIN(2, 25, w.fr);
    TEST_ASSERT_INT_WITHIN(2, 76, w.rl);
    TEST_ASSERT_INT_WITHIN(2, 127, w.rr);
}

// Saturated combined motion: FL raw = 300 → must cap at exactly +255; the
// others just have to stay in range and keep their signs (clamp OR normalize
// both satisfy this).
void test_saturated_combined_motion_stays_in_range() {
    WheelSpeeds w = mix(100, 100, 100);
    TEST_ASSERT_EQUAL_INT16(kMaxPwm, w.fl);
    TEST_ASSERT_TRUE(w.fr <= 0);   // raw -100
    TEST_ASSERT_TRUE(w.rl >= 0);   // raw +100
    TEST_ASSERT_TRUE(w.rr >= 0);   // raw +100
    assert_in_range(w);
}

// Diagonal (forward-right): FR and RL should be near zero, FL and RR drive.
void test_diagonal_forward_right() {
    WheelSpeeds w = mix(50, 50, 0);
    TEST_ASSERT_INT_WITHIN(2, kMaxPwm, w.fl);
    TEST_ASSERT_INT_WITHIN(2, 0, w.fr);
    TEST_ASSERT_INT_WITHIN(2, 0, w.rl);
    TEST_ASSERT_INT_WITHIN(2, kMaxPwm, w.rr);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_zero_input_is_all_stop);
    RUN_TEST(test_full_forward_all_wheels_full_ahead);
    RUN_TEST(test_full_reverse_all_wheels_full_astern);
    RUN_TEST(test_half_forward_is_proportional);
    RUN_TEST(test_strafe_right_diagonal_pairs);
    RUN_TEST(test_strafe_left_diagonal_pairs);
    RUN_TEST(test_rotate_clockwise_sides_oppose);
    RUN_TEST(test_combined_motion_ratios);
    RUN_TEST(test_saturated_combined_motion_stays_in_range);
    RUN_TEST(test_diagonal_forward_right);
    return UNITY_END();
}
