// Host-run tests for the dance program parser: `pio test -e native`
//
// Legacy 4-field timed steps must parse exactly as dance::setProgram always
// did (clamping, 50..15000ms, all-or-nothing on error); the new "t,deg,w"
// turn form is pinned down here too.

#include <unity.h>

#include <cstring>

#include "dancescript.h"

using dancescript::kMaxSteps;
using dancescript::kMaxTotalMs;
using dancescript::parse;
using dancescript::Step;

constexpr uint32_t kTimeout = 6000;

static Step steps[kMaxSteps];

void test_legacy_single_step() {
    TEST_ASSERT_EQUAL_INT(1, parse("50,-30,10,1500", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_FALSE(steps[0].turn);
    TEST_ASSERT_EQUAL_INT8(50, steps[0].vx);
    TEST_ASSERT_EQUAL_INT8(-30, steps[0].vy);
    TEST_ASSERT_EQUAL_INT8(10, steps[0].w);
    TEST_ASSERT_EQUAL_UINT32(1500, steps[0].ms);
}

void test_legacy_multi_step_and_spaces() {
    TEST_ASSERT_EQUAL_INT(3,
        parse("100,0,0,500; 0,100,0,500 ;0,0,-100,500", steps, kMaxSteps,
              kTimeout));
    TEST_ASSERT_EQUAL_INT8(100, steps[1].vy);
    TEST_ASSERT_EQUAL_INT8(-100, steps[2].w);
}

void test_legacy_clamping() {
    TEST_ASSERT_EQUAL_INT(1, parse("200,-200,150,20", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT8(100, steps[0].vx);
    TEST_ASSERT_EQUAL_INT8(-100, steps[0].vy);
    TEST_ASSERT_EQUAL_INT8(100, steps[0].w);
    TEST_ASSERT_EQUAL_UINT32(50, steps[0].ms);  // ms floor
    TEST_ASSERT_EQUAL_INT(1, parse("0,0,0,99999", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_UINT32(15000, steps[0].ms);  // ms ceiling
}

void test_turn_step_cw() {
    TEST_ASSERT_EQUAL_INT(1, parse("t,90,40", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_TRUE(steps[0].turn);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, steps[0].turn_deg);
    TEST_ASSERT_EQUAL_INT8(40, steps[0].w);       // CW turn → positive omega
    TEST_ASSERT_EQUAL_UINT32(kTimeout, steps[0].ms);
    TEST_ASSERT_EQUAL_INT8(0, steps[0].vx);
    TEST_ASSERT_EQUAL_INT8(0, steps[0].vy);
}

void test_turn_step_ccw_direction_from_deg() {
    // Speed field is magnitude; the sign of deg picks the direction.
    TEST_ASSERT_EQUAL_INT(1, parse("t,-90,40", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT8(-40, steps[0].w);
    TEST_ASSERT_EQUAL_INT(1, parse("T,-45,-60", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT8(-60, steps[0].w);
}

void test_turn_clamping() {
    TEST_ASSERT_EQUAL_INT(1, parse("t,900,40", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 720.0f, steps[0].turn_deg);
    TEST_ASSERT_EQUAL_INT(1, parse("t,90,5", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT8(10, steps[0].w);   // speed floor
    TEST_ASSERT_EQUAL_INT(1, parse("t,90,999", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT8(100, steps[0].w);  // speed ceiling
}

void test_mixed_program() {
    TEST_ASSERT_EQUAL_INT(4, parse("50,0,0,1500;t,90,40;0,0,0,300;t,-180,60",
                                   steps, kMaxSteps, kTimeout));
    TEST_ASSERT_FALSE(steps[0].turn);
    TEST_ASSERT_TRUE(steps[1].turn);
    TEST_ASSERT_FALSE(steps[2].turn);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -180.0f, steps[3].turn_deg);
}

void test_turn_timeout_counts_toward_total_cap() {
    // 20 turns × 6000ms timeout = 120000 = exactly kMaxTotalMs → OK;
    // adding one more step must fail.
    char prog[512] = "";
    for (int i = 0; i < 20; i++) strcat(prog, "t,90,40;");
    prog[strlen(prog) - 1] = '\0';
    TEST_ASSERT_EQUAL_INT(20, parse(prog, steps, kMaxSteps, kTimeout));
    strcat(prog, ";0,0,0,50");
    TEST_ASSERT_EQUAL_INT(-1, parse(prog, steps, kMaxSteps, kTimeout));
}

void test_garbage_rejected() {
    TEST_ASSERT_EQUAL_INT(-1, parse("", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT(-1, parse(nullptr, steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT(-1, parse("1,2,3", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT(-1, parse("banana", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT(-1, parse("t,90", steps, kMaxSteps, kTimeout));
    TEST_ASSERT_EQUAL_INT(-1, parse("50,0,0,500;oops;t,90,40", steps,
                                    kMaxSteps, kTimeout));
}

void test_step_count_limit() {
    char prog[2048] = "";
    for (int i = 0; i < kMaxSteps + 1; i++) strcat(prog, "0,0,0,50;");
    TEST_ASSERT_EQUAL_INT(-1, parse(prog, steps, kMaxSteps, kTimeout));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_legacy_single_step);
    RUN_TEST(test_legacy_multi_step_and_spaces);
    RUN_TEST(test_legacy_clamping);
    RUN_TEST(test_turn_step_cw);
    RUN_TEST(test_turn_step_ccw_direction_from_deg);
    RUN_TEST(test_turn_clamping);
    RUN_TEST(test_mixed_program);
    RUN_TEST(test_turn_timeout_counts_toward_total_cap);
    RUN_TEST(test_garbage_rejected);
    RUN_TEST(test_step_count_limit);
    return UNITY_END();
}
