// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// TEST-ONLY reference model of an exclusive FF800 configuration lease.
// No DriverKit dependency, no bus writes, and no production wiring.
// All calls must be serialized by the simulated owner. A real integration
// must make admission atomic with SessionScheduler::Submit/Reconcile.
#pragma once

#include "FF800ControlSessionGate.hpp"

#include <cstdint>
#include <optional>

namespace ASFW::Audio::RME {

struct FF800SimulatedLeaseToken {
    uint64_t serial{};
    uint64_t epoch{};
    uint32_t generation{};
    [[nodiscard]] bool operator==(const FF800SimulatedLeaseToken&) const = default;
};

class FF800SimulatedConfigLease final {
public:
    /// Begin a simulated exclusive full-image configuration window only when
    /// the audio session is quiescent. No hardware write follows this method.
    [[nodiscard]] std::optional<FF800SimulatedLeaseToken>
    Acquire(const FF800ControlSessionConditions& session) noexcept {
        if (active_ || audioBusy_ || !CanPreviewFF800ControlSession(session)) return std::nullopt;
        const FF800SimulatedLeaseToken token{++serial_, epoch_, session.routeGeneration};
        active_ = token;
        return token;
    }

    /// Audio Start/Stop/Configure may not overlap a simulated held lease.
    /// The *real* scheduler does not call this; this tests a required future
    /// admission contract, not a behavior the production driver provides.
    [[nodiscard]] bool BeginAudioMutation() noexcept {
        if (active_ || audioBusy_) return false;
        audioBusy_ = true;
        return true;
    }

    /// Audio has priority over control. In the simulated integration contract,
    /// an incoming Start/Stop/Configure invalidates any outstanding control
    /// token immediately; no callback or transport completion is awaited.
    /// Production code must coordinate this with the session's own lock and
    /// with the control dispatch/cancellation lifecycle.
    void PreemptForAudioMutation() noexcept {
        ++epoch_;
        active_.reset();
        audioBusy_ = true;
    }

    /// Retirement/teardown must invalidate a pending control without waiting
    /// for its completion. Further acquisition is refused until a fresh
    /// session and route are supplied by the caller.
    void PreemptForDeviceLoss() noexcept {
        ++epoch_;
        active_.reset();
        audioBusy_ = false;
    }

    void EndAudioMutation() noexcept { audioBusy_ = false; }

    [[nodiscard]] bool Holds(FF800SimulatedLeaseToken token) const noexcept {
        return active_.has_value() && *active_ == token;
    }

    [[nodiscard]] bool Release(FF800SimulatedLeaseToken token) noexcept {
        if (!Holds(token)) return false;
        active_.reset();
        return true;
    }

    /// A reset/retirement invalidates every token, including a token whose
    /// serial and FireWire generation are later reused.
    void Invalidate() noexcept {
        ++epoch_;
        active_.reset();
    }

    [[nodiscard]] bool AudioBusy() const noexcept { return audioBusy_; }

private:
    uint64_t serial_{0};
    uint64_t epoch_{1};
    bool audioBusy_{false};
    std::optional<FF800SimulatedLeaseToken> active_{};
};

} // namespace ASFW::Audio::RME
