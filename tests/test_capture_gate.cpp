#include "capture_gate.h"

#include <catch2/catch_test_macros.hpp>

using raysniff::CaptureGate;
using raysniff::GateAction;

namespace {

constexpr bool kDspOn = true;
constexpr bool kDspOff = false;
constexpr bool kOpen = true;
constexpr bool kClosed = false;
constexpr bool kFailed = true;
constexpr bool kHealthy = false;

} // namespace

TEST_CASE("capture gate: opens only with DSP on and a positive int", "[gate]") {
    CaptureGate gate;
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::none);   // not requested

    gate.set_requested(true);
    CHECK(gate.update(kDspOff, kClosed, kHealthy) == GateAction::none);  // DSP off
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
    gate.open_result(true);
    CHECK(gate.update(kDspOn, kOpen, kHealthy) == GateAction::none);     // stays open
}

TEST_CASE("capture gate: closes on DSP off or int 0", "[gate]") {
    CaptureGate gate;
    gate.set_requested(true);
    REQUIRE(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
    gate.open_result(true);

    CHECK(gate.update(kDspOff, kOpen, kHealthy) == GateAction::close);
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);   // DSP back on
    gate.open_result(true);

    gate.set_requested(false);
    CHECK(gate.update(kDspOn, kOpen, kHealthy) == GateAction::close);
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::none);
}

TEST_CASE("capture gate: a failed open is not retried at every poll", "[gate]") {
    CaptureGate gate;
    gate.set_requested(true);
    REQUIRE(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
    gate.open_result(false);

    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::none);
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::none);
}

TEST_CASE("capture gate: each trigger allows one more open after a failure", "[gate]") {
    CaptureGate gate;
    gate.set_requested(true);
    REQUIRE(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
    gate.open_result(false);

    SECTION("a positive int, even without 0 in between") {
        gate.set_requested(true);
    }
    SECTION("DSP turned off and on") {
        CHECK(gate.update(kDspOff, kClosed, kHealthy) == GateAction::none);
    }
    SECTION("@device or @filter changed") {
        gate.config_changed();
    }
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
}

TEST_CASE("capture gate: a failed capture thread is closed once and not reopened", "[gate]") {
    CaptureGate gate;
    gate.set_requested(true);
    REQUIRE(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
    gate.open_result(true);

    CHECK(gate.update(kDspOn, kOpen, kFailed) == GateAction::close_after_failure);
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::none);

    gate.set_requested(true);
    CHECK(gate.update(kDspOn, kClosed, kHealthy) == GateAction::open);
}
