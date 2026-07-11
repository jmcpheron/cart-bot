// Host-run tests for the yaw heading control: `pio test -e native`
//
// These tests ARE the spec for heading::YawHold and heading::turnComplete.
// They pin down the sign convention (yaw and omega CW-positive: yaw drifted
// clockwise of target → negative/CCW trim), the capture/release rules, and
// the clamping. Gains here are test gains set via configure(), not the
// robot's tuned values.

#include <unity.h>
#include "heading.h"

using heading::turnComplete;
using heading::wrap180;
using heading::YawHold;
using heading::YawHoldConfig;

// kp=2, ki=1/s, i_clamp=15, out_clamp=30, deadband=0.5°
static YawHold make_hold() {
    YawHold h;
    h.configure(YawHoldConfig{2.0f, 1.0f, 15.0f, 30.0f, 0.5f});
    return h;
}

constexpr float kDt = 0.02f;  // motor task tick

void test_wrap180_folds_into_range() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, wrap180(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -170.0f, wrap180(190.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 170.0f, wrap180(-190.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, wrap180(720.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -180.0f, wrap180(180.0f));
}

void test_capture_tick_has_zero_error() {
    YawHold h = make_hold();
    // First hold tick captures yaw as target → no error, no trim.
    TEST_ASSERT_EQUAL_INT(0, h.update(true, 0, true, 37.0f, kDt));
    TEST_ASSERT_TRUE(h.holding());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 37.0f, h.targetYaw());
}

// CW-positive convention: yaw drifted +5° (clockwise of target) needs a
// counter-clockwise (negative) trim, and vice versa.
void test_p_term_sign_opposes_drift() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);  // capture at 0
    TEST_ASSERT_TRUE(h.update(true, 0, true, 5.0f, kDt) < 0);
    h.reset();
    h.update(true, 0, true, 0.0f, kDt);
    TEST_ASSERT_TRUE(h.update(true, 0, true, -5.0f, kDt) > 0);
}

void test_commanded_rotation_releases_hold() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    h.update(true, 0, true, 5.0f, kDt);
    TEST_ASSERT_EQUAL_INT(0, h.update(true, 40, true, 5.0f, kDt));
    TEST_ASSERT_FALSE(h.holding());
}

void test_idle_releases_hold() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    TEST_ASSERT_EQUAL_INT(0, h.update(true, 0, false, 5.0f, kDt));
    TEST_ASSERT_FALSE(h.holding());
}

void test_disabled_releases_hold() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    TEST_ASSERT_EQUAL_INT(0, h.update(false, 0, true, 5.0f, kDt));
    TEST_ASSERT_FALSE(h.holding());
}

void test_recapture_uses_new_heading() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);   // hold at 0
    h.update(true, 50, true, 90.0f, kDt); // operator turns → release
    h.update(true, 0, true, 90.0f, kDt);  // new hold captures 90
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, h.targetYaw());
    TEST_ASSERT_EQUAL_INT(0, h.update(true, 0, true, 90.0f, kDt));
}

void test_deadband_zeroes_small_error() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    TEST_ASSERT_EQUAL_INT(0, h.update(true, 0, true, 0.3f, kDt));
}

void test_output_clamped() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    // 100° error × kp=2 = 200 raw → clamps at out_clamp=30.
    TEST_ASSERT_EQUAL_INT(-30, h.update(true, 0, true, 100.0f, kDt));
    TEST_ASSERT_EQUAL_INT(30, h.update(true, 0, true, -100.0f, kDt));
}

// Constant unsaturated error: integrator grows tick over tick, but never
// past i_clamp — total output stays ≤ kp*e + i_clamp even after many ticks.
void test_integrator_accumulates_and_clamps() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    const int first = h.update(true, 0, true, 5.0f, kDt);   // p=-10, tiny I
    int last = first;
    for (int i = 0; i < 500; i++) last = h.update(true, 0, true, 5.0f, kDt);
    TEST_ASSERT_TRUE(last < first);          // integrator added push
    TEST_ASSERT_TRUE(last >= -(10 + 15));    // p + i_clamp bound
}

// While the output is saturated the integrator must freeze: after the error
// vanishes, the trim must return to ~0 immediately, not unwind a wound-up
// integrator.
void test_no_windup_while_saturated() {
    YawHold h = make_hold();
    h.update(true, 0, true, 0.0f, kDt);
    for (int i = 0; i < 500; i++) h.update(true, 0, true, 100.0f, kDt);
    TEST_ASSERT_EQUAL_INT(0, h.update(true, 0, true, 0.0f, kDt));
}

void test_turn_complete_cw() {
    TEST_ASSERT_FALSE(turnComplete(0.0f, 89.0f, 90.0f, 0.0f));
    TEST_ASSERT_TRUE(turnComplete(0.0f, 90.0f, 90.0f, 0.0f));
    TEST_ASSERT_TRUE(turnComplete(0.0f, 95.0f, 90.0f, 0.0f));  // overshoot
}

void test_turn_complete_ccw() {
    TEST_ASSERT_FALSE(turnComplete(0.0f, -89.0f, -90.0f, 0.0f));
    TEST_ASSERT_TRUE(turnComplete(0.0f, -90.0f, -90.0f, 0.0f));
    // Rotating the wrong way must never complete a CCW turn.
    TEST_ASSERT_FALSE(turnComplete(0.0f, 90.0f, -90.0f, 0.0f));
}

void test_turn_complete_multiturn_and_offset_start() {
    TEST_ASSERT_FALSE(turnComplete(45.0f, 764.0f, 720.0f, 0.0f));
    TEST_ASSERT_TRUE(turnComplete(45.0f, 765.0f, 720.0f, 0.0f));
}

void test_turn_complete_stop_early() {
    TEST_ASSERT_TRUE(turnComplete(0.0f, 85.0f, 90.0f, 5.0f));
    TEST_ASSERT_FALSE(turnComplete(0.0f, 84.0f, 90.0f, 5.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_wrap180_folds_into_range);
    RUN_TEST(test_capture_tick_has_zero_error);
    RUN_TEST(test_p_term_sign_opposes_drift);
    RUN_TEST(test_commanded_rotation_releases_hold);
    RUN_TEST(test_idle_releases_hold);
    RUN_TEST(test_disabled_releases_hold);
    RUN_TEST(test_recapture_uses_new_heading);
    RUN_TEST(test_deadband_zeroes_small_error);
    RUN_TEST(test_output_clamped);
    RUN_TEST(test_integrator_accumulates_and_clamps);
    RUN_TEST(test_no_windup_while_saturated);
    RUN_TEST(test_turn_complete_cw);
    RUN_TEST(test_turn_complete_ccw);
    RUN_TEST(test_turn_complete_multiturn_and_offset_start);
    RUN_TEST(test_turn_complete_stop_early);
    return UNITY_END();
}
