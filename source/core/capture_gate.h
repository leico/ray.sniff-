// Decides when the Max object opens and closes the capture (design.md 7.4).

#pragma once

namespace raysniff {

enum class GateAction {
    none,
    open,                 // open the capture; report the result with open_result()
    close,                // DSP off or `int 0`: close quietly
    close_after_failure,  // the capture thread failed: report its error, then close
};

// The capture is open while "DSP of this object is on" and "the last int was
// positive". After a failure (an open that failed, or a capture thread that
// failed) it is not opened again until a trigger: a positive int, DSP turned
// on, or a change of @device / @filter. So an error is reported once, not at
// every poll.
//
// Pure logic without Max, so that it can be tested. Main thread only.
class CaptureGate {
public:
    // The int message. A positive value is also a trigger, even when the
    // capture was already requested.
    void set_requested(bool requested) {
        requested_ = requested;
        if (requested) armed_ = true;
    }

    // @device or @filter changed.
    void config_changed() { armed_ = true; }

    // The result of an open that update() asked for.
    void open_result(bool ok) {
        if (!ok) armed_ = false;
    }

    // dsp_on: DSP state of this object. open: the session is open.
    // failed: the capture thread stopped with an error.
    GateAction update(bool dsp_on, bool open, bool failed) {
        if (dsp_on && !dsp_on_) armed_ = true;  // DSP turned on
        dsp_on_ = dsp_on;
        const bool wanted = requested_ && dsp_on;

        if (open) {
            if (failed) {
                armed_ = false;
                return GateAction::close_after_failure;
            }
            return wanted ? GateAction::none : GateAction::close;
        }
        return (wanted && armed_) ? GateAction::open : GateAction::none;
    }

private:
    bool requested_ = false;
    bool dsp_on_ = false;  // at the previous update(), to see DSP turning on
    bool armed_ = true;    // false after a failure, until the next trigger
};

} // namespace raysniff
