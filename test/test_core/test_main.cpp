#include <unity.h>
#include "boat/Controller.h"
#include "boat/Motion.h"
#include "boat/Lights.h"
#include <limits>
using namespace boat;

void setUp() {}
void tearDown() {}
Inputs good() {
    Inputs in;
    in.batteryValid = in.batteryRecovered = in.hardwareReady = true;
    in.cameraReady = in.detectorReady = true;
    in.detection.frame = 1; in.detection.capturedAt = 100;
    in.detection.healthy = true;
    return in;
}
bool send(Controller& c, Action a, uint32_t now, const Inputs& in,
          int left = 0, int right = 0, int conveyor = 0) {
    Command cmd;
    cmd.action = a; cmd.epoch = c.epoch(); cmd.sequence = c.sequence() + 1;
    cmd.left = left; cmd.right = right; cmd.conveyor = conveyor;
    return c.command(cmd, now, in);
}
void isStopped(const Output& out) {
    TEST_ASSERT_EQUAL_INT(0, out.left); TEST_ASSERT_EQUAL_INT(0, out.right); TEST_ASSERT_EQUAL_INT(0, out.conveyor);
}
void test_boot_requires_explicit_arm() {
    Controller c; auto in = good();
    isStopped(c.tick(100, in));
    TEST_ASSERT_FALSE(send(c, Action::Drive, 100, in, 100, 100));
    TEST_ASSERT_TRUE(send(c, Action::Manual, 100, in));
    isStopped(c.tick(101, in));
}
void test_deadman_stops_all_outputs_and_expires_epoch() {
    Controller c; auto in = good(); send(c, Action::Manual, 100, in);
    send(c, Action::Drive, 101, in, 150, 150, 255);
    TEST_ASSERT_EQUAL_INT(150, c.tick(1100, in).left);
    const auto epoch = c.epoch();
    isStopped(c.tick(1101, in));
    TEST_ASSERT_EQUAL_INT(int(Reason::LinkTimeout), int(c.reason()));
    TEST_ASSERT_NOT_EQUAL(epoch, c.epoch());
}
void test_late_drive_cannot_resurrect_session_before_tick() {
    Controller c; auto in = good(); send(c, Action::Manual, 100, in);
    TEST_ASSERT_FALSE(send(c, Action::Drive, 1100, in, 150,150));
    isStopped(c.tick(1100,in));
}
void test_stop_invalidates_queued_commands_and_stale_rearm() {
    Controller c; auto in = good(); send(c, Action::Manual, 100, in);
    Command old; old.action = Action::Drive; old.epoch = c.epoch(); old.sequence = 99; old.left = old.right = 100;
    send(c, Action::Stop, 200, in);
    TEST_ASSERT_FALSE(c.command(old, 201, in));
    old.action = Action::Auto;
    TEST_ASSERT_FALSE(c.command(old, 202, in));
    isStopped(c.tick(203,in));
}
void test_out_of_order_commands_do_not_restore_old_motion() {
    Controller c; auto in = good(); send(c,Action::Manual,100,in);
    Command cmd; cmd.action=Action::Drive; cmd.epoch=c.epoch(); cmd.sequence=2;
    TEST_ASSERT_TRUE(c.command(cmd,101,in));
    cmd.sequence=1; cmd.left=cmd.right=100;
    TEST_ASSERT_FALSE(c.command(cmd,102,in));
    isStopped(c.tick(103,in));
}
void test_manual_heartbeat_does_not_extend_movement() {
    Controller c; auto in = good(); send(c,Action::Manual,100,in);
    send(c,Action::Drive,101,in,100,100);
    TEST_ASSERT_FALSE(send(c,Action::Heartbeat,900,in));
    isStopped(c.tick(1101,in));
}
void test_invalid_command_rejected_without_renewal() {
    Controller c; auto in=good(); send(c,Action::Manual,100,in);
    TEST_ASSERT_FALSE(send(c,Action::Drive,900,in,256,100));
    isStopped(c.tick(1100,in));
}
void test_drive_speed_clamped() {
    Controller c; auto in=good(); send(c,Action::Manual,100,in);
    send(c,Action::Drive,110,in,255,-255);
    auto out=c.tick(120,in); TEST_ASSERT_EQUAL_INT(180,out.left); TEST_ASSERT_EQUAL_INT(-180,out.right);
}
void test_low_battery_fault_is_latched_and_stop_does_not_clear_it() {
    Controller c; auto in=good(); send(c,Action::Manual,100,in);
    send(c,Action::Drive,110,in,100,100,255); in.batteryLow=true; in.batteryRecovered=false;
    isStopped(c.tick(120,in)); TEST_ASSERT_EQUAL_INT(int(State::Fault),int(c.state()));
    send(c,Action::Stop,130,in);
    TEST_ASSERT_EQUAL_INT(int(State::Fault),int(c.state()));
    TEST_ASSERT_FALSE(send(c,Action::ClearFault,140,in));
    in.batteryLow=false; in.batteryRecovered=true;
    TEST_ASSERT_TRUE(send(c,Action::ClearFault,150,in));
    isStopped(c.tick(160,in));
}
void test_missing_adc_blocks_live_but_not_bench() {
    auto in=good(); in.batteryValid=false; Controller live;
    TEST_ASSERT_FALSE(send(live,Action::Manual,100,in));
    Settings cfg; cfg.requireBattery=false; Controller bench(cfg);
    TEST_ASSERT_TRUE(send(bench,Action::Manual,100,in));
}
void test_jam_stops_and_latches_fault() {
    Controller c; auto in=good(); send(c,Action::Manual,100,in); in.jam=true;
    isStopped(c.tick(101,in)); TEST_ASSERT_EQUAL_INT(int(Reason::Jam),int(c.reason()));
}
void test_full_bin_stops_immediately() {
    Controller controller;
    auto inputs = good();
    TEST_ASSERT_TRUE(send(controller, Action::Manual, 100, inputs));
    inputs.binFull = true;
    isStopped(controller.tick(120, inputs));
    TEST_ASSERT_EQUAL_INT(int(Reason::BinFull), int(controller.reason()));
    TEST_ASSERT_FALSE(send(controller, Action::ClearFault, 130, inputs));
}

void test_ultrasonic_conveyor_runs_one_timed_revolution() {
    Controller controller;
    auto inputs = good();
    TEST_ASSERT_TRUE(send(controller, Action::Manual, 100, inputs));
    inputs.obstacleNear = true;
    for (uint32_t now = 120; now < 3120; now += 500) {
        TEST_ASSERT_TRUE(send(controller, Action::Drive, now, inputs, 80, 80, -180));
        const auto output = controller.tick(now, inputs);
        TEST_ASSERT_EQUAL_INT(0, output.left);
        TEST_ASSERT_EQUAL_INT(0, output.right);
        TEST_ASSERT_EQUAL_INT(180, output.conveyor);
    }
    TEST_ASSERT_EQUAL_INT(int(UltrasonicCycle::Running), int(controller.ultrasonicCycle()));
    TEST_ASSERT_EQUAL_INT(180, controller.tick(3119, inputs).conveyor);
    isStopped(controller.tick(3120, inputs));
    TEST_ASSERT_EQUAL_INT(int(UltrasonicCycle::Finished), int(controller.ultrasonicCycle()));
    TEST_ASSERT_EQUAL_INT(int(Reason::Obstacle), int(controller.reason()));
    TEST_ASSERT_FALSE(send(controller, Action::ClearFault, 3130, inputs));
    isStopped(controller.tick(3140, inputs));
}

void test_stop_cancels_ultrasonic_conveyor() {
    Controller controller;
    auto inputs = good();
    TEST_ASSERT_TRUE(send(controller, Action::Manual, 100, inputs));
    inputs.obstacleNear = true;
    TEST_ASSERT_EQUAL_INT(180, controller.tick(120, inputs).conveyor);
    TEST_ASSERT_TRUE(send(controller, Action::Stop, 130, inputs));
    isStopped(controller.tick(130, inputs));
    TEST_ASSERT_EQUAL_INT(int(UltrasonicCycle::Interrupted), int(controller.ultrasonicCycle()));
}
void test_auto_requires_fresh_real_detector_output() {
    Controller c; auto in=good(); in.detectorReady=false;
    TEST_ASSERT_FALSE(send(c,Action::Auto,100,in));
    in.detectorReady=true; in.detection.healthy=false;
    TEST_ASSERT_FALSE(send(c,Action::Auto,100,in));
    in.detection.healthy=true;
    TEST_ASSERT_FALSE(send(c,Action::Auto,2000,in));
}
void test_confirmation_requires_distinct_frames() {
    Controller c; auto in=good(); in.detection.found=true; in.detection.confidence=.9f;
    send(c,Action::Auto,100,in);
    for(int i=0;i<10;++i) isStopped(c.tick(101+i,in));
    in.detection.frame=2; isStopped(c.tick(120,in));
    in.detection.frame=3; TEST_ASSERT_GREATER_THAN(0,c.tick(140,in).left);
}
void test_left_target_turns_left_and_loss_stops_immediately() {
    Settings cfg; cfg.confirmFrames=1; Controller c(cfg); auto in=good();
    in.detection.found=true; in.detection.x=.1f; in.detection.confidence=.9f;
    send(c,Action::Auto,100,in);
    auto out=c.tick(110,in); TEST_ASSERT_LESS_THAN(out.right,out.left);
    in.detection.frame=2; in.detection.found=false;
    isStopped(c.tick(120,in));
}
void test_stale_vision_cuts_outputs_even_with_heartbeats() {
    Settings cfg; cfg.confirmFrames=1; Controller c(cfg); auto in=good();
    send(c,Action::Auto,100,in); send(c,Action::Heartbeat,900,in);
    send(c,Action::Heartbeat,1500,in);
    isStopped(c.tick(1601,in)); TEST_ASSERT_EQUAL_INT(int(Reason::VisionStale),int(c.reason()));
}
void test_nan_detection_is_not_a_target() {
    Settings cfg; cfg.confirmFrames=1; Controller c(cfg); auto in=good();
    in.detection.found=true; in.detection.confidence=.99f; in.detection.x=std::numeric_limits<float>::quiet_NaN();
    send(c,Action::Auto,100,in); c.tick(110,in);
    TEST_ASSERT_EQUAL_INT(int(State::Search),int(c.state()));
}
void test_collect_has_finite_duration_and_counts_attempt_only() {
    Settings cfg; cfg.confirmFrames=1; cfg.collectMs=400; Controller c(cfg); auto in=good();
    in.detection.found=true; in.detection.confidence=.9f; in.detection.y=.9f;
    send(c,Action::Auto,100,in);
    TEST_ASSERT_GREATER_THAN(0,c.tick(110,in).conveyor);
    in.detection.found=false; in.detection.frame=2;
    TEST_ASSERT_GREATER_THAN(0,c.tick(200,in).conveyor);
    isStopped(c.tick(510,in)); TEST_ASSERT_EQUAL_UINT(1,c.collectionCycles());
    TEST_ASSERT_EQUAL_INT(int(State::Stop),int(c.state()));
}
void test_manual_conveyor_runs_past_twelve_seconds_with_drive_updates() {
    Controller controller;
    auto inputs = good();
    TEST_ASSERT_TRUE(send(controller, Action::Manual, 100, inputs));
    TEST_ASSERT_FALSE(send(controller, Action::Drive, 105, inputs, 0, 0, -256));
    for (uint32_t now = 110; now <= 13110; now += 500) {
        TEST_ASSERT_TRUE(send(controller, Action::Drive, now, inputs, 80, 80, -180));
        auto output = controller.tick(now, inputs);
        TEST_ASSERT_EQUAL_INT(80, output.left);
        TEST_ASSERT_EQUAL_INT(80, output.right);
        TEST_ASSERT_EQUAL_INT(-180, output.conveyor);
    }
    isStopped(controller.tick(14110, inputs));
    TEST_ASSERT_EQUAL_INT(int(Reason::LinkTimeout), int(controller.reason()));
}
void test_search_timeout_with_fresh_frames() {
    Settings cfg; cfg.searchTimeoutMs=500; Controller c(cfg); auto in=good(); send(c,Action::Auto,100,in);
    in.detection.frame=2; in.detection.capturedAt=600;
    isStopped(c.tick(600,in)); TEST_ASSERT_EQUAL_INT(int(Reason::SearchTimeout),int(c.reason()));
}
void test_approach_timeout_with_fresh_frames() {
    Settings cfg; cfg.confirmFrames=1; cfg.approachTimeoutMs=500; Controller c(cfg); auto in=good();
    in.detection.found=true; in.detection.confidence=.9f;
    send(c,Action::Auto,100,in); c.tick(110,in);
    in.detection.frame=2; in.detection.capturedAt=610;
    isStopped(c.tick(610,in)); TEST_ASSERT_EQUAL_INT(int(Reason::ApproachTimeout),int(c.reason()));
}
void test_millis_wrap_preserves_deadman() {
    Controller c; auto in=good(); const uint32_t start=0xffffff00u;
    send(c,Action::Manual,start,in); send(c,Action::Drive,start+1,in,100,100);
    TEST_ASSERT_EQUAL_INT(100,c.tick(start+900,in).left);
    isStopped(c.tick(start+1001,in));
}
void test_motor_reversal_waits_at_zero() {
    MotorRamp r(20,80); TEST_ASSERT_EQUAL_INT(20,r.update(100,0));
    TEST_ASSERT_EQUAL_INT(40,r.update(100,20));
    TEST_ASSERT_EQUAL_INT(20,r.update(-100,40));
    TEST_ASSERT_EQUAL_INT(0,r.update(-100,60));
    TEST_ASSERT_EQUAL_INT(0,r.update(-100,139));
    TEST_ASSERT_EQUAL_INT(-20,r.update(-100,140));
}
void test_emergency_zero_bypasses_motor_ramp() {
    MotorRamp r(20,80); for(int t=0;t<200;t+=20) r.update(180,t);
    TEST_ASSERT_EQUAL_INT(0,r.update(0,200));
    TEST_ASSERT_EQUAL_INT(0,r.update(-100,220));
    TEST_ASSERT_EQUAL_INT(-20,r.update(-100,280));
}
void test_lights_reject_stale_request_after_stop() {
    Lights lights;
    const uint32_t oldRevision = lights.state().revision;
    lights.off();
    TEST_ASSERT_FALSE(lights.set(oldRevision, true, RgbColor::White));
    TEST_ASSERT_FALSE(lights.state().red);
    TEST_ASSERT_EQUAL_INT(int(RgbColor::Off), int(lights.state().rgb));
}
void test_lights_compare_and_swap_prevents_two_clients_overwriting() {
    Lights lights;
    const uint32_t revision = lights.state().revision;
    TEST_ASSERT_TRUE(lights.set(revision, true, RgbColor::Yellow));
    TEST_ASSERT_FALSE(lights.set(revision, false, RgbColor::Blue));
    TEST_ASSERT_TRUE(lights.state().red);
    TEST_ASSERT_EQUAL_INT(int(RgbColor::Yellow), int(lights.state().rgb));
}
void test_lights_invalid_color_does_not_change_revision() {
    Lights lights;
    TEST_ASSERT_FALSE(lights.set(0, true, static_cast<RgbColor>(99)));
    TEST_ASSERT_EQUAL_UINT32(0, lights.state().revision);
    TEST_ASSERT_FALSE(lights.state().red);
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_requires_explicit_arm);
    RUN_TEST(test_deadman_stops_all_outputs_and_expires_epoch);
    RUN_TEST(test_late_drive_cannot_resurrect_session_before_tick);
    RUN_TEST(test_stop_invalidates_queued_commands_and_stale_rearm);
    RUN_TEST(test_out_of_order_commands_do_not_restore_old_motion);
    RUN_TEST(test_manual_heartbeat_does_not_extend_movement);
    RUN_TEST(test_invalid_command_rejected_without_renewal);
    RUN_TEST(test_drive_speed_clamped);
    RUN_TEST(test_low_battery_fault_is_latched_and_stop_does_not_clear_it);
    RUN_TEST(test_missing_adc_blocks_live_but_not_bench);
    RUN_TEST(test_jam_stops_and_latches_fault);
    RUN_TEST(test_full_bin_stops_immediately);
    RUN_TEST(test_ultrasonic_conveyor_runs_one_timed_revolution);
    RUN_TEST(test_stop_cancels_ultrasonic_conveyor);
    RUN_TEST(test_auto_requires_fresh_real_detector_output);
    RUN_TEST(test_confirmation_requires_distinct_frames);
    RUN_TEST(test_left_target_turns_left_and_loss_stops_immediately);
    RUN_TEST(test_stale_vision_cuts_outputs_even_with_heartbeats);
    RUN_TEST(test_nan_detection_is_not_a_target);
    RUN_TEST(test_collect_has_finite_duration_and_counts_attempt_only);
    RUN_TEST(test_manual_conveyor_runs_past_twelve_seconds_with_drive_updates);
    RUN_TEST(test_search_timeout_with_fresh_frames);
    RUN_TEST(test_approach_timeout_with_fresh_frames);
    RUN_TEST(test_millis_wrap_preserves_deadman);
    RUN_TEST(test_motor_reversal_waits_at_zero);
    RUN_TEST(test_emergency_zero_bypasses_motor_ramp);
    RUN_TEST(test_lights_reject_stale_request_after_stop);
    RUN_TEST(test_lights_compare_and_swap_prevents_two_clients_overwriting);
    RUN_TEST(test_lights_invalid_color_does_not_change_revision);
    return UNITY_END();
}
