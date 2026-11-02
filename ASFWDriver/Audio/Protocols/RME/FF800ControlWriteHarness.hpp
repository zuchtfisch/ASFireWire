// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// TEST-ONLY transport boundary for the future FF800 controls writer.
// This deliberately has no FireWire bus dependency and is not attached to the
// app or device. A caller supplies a fake asynchronous dispatch function.
// Device acknowledgements are transport results, never hardware readback.
#pragma once

#include "FF800ControlWriteState.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ASFW::Audio::RME {

struct FF800WriteEnvelope {
    uint64_t requestId{};
    uint32_t generation{};
    ConfigWords words{};
};

/// A callback-driven transport simulation. No hardware I/O can occur unless
/// somebody explicitly injects a transport. Not used by the production driver.
class FF800ControlWriteHarness final {
public:
    using Completion = std::function<void(bool accepted)>;
    using Dispatch = std::function<void(const FF800WriteEnvelope&, Completion)>;

    explicit FF800ControlWriteHarness(Dispatch dispatch)
        : shared_(std::make_shared<Shared>()), dispatch_(std::move(dispatch)) {}

    /// Reject concurrent requests. A prior ambiguous result MUST be resolved
    /// externally before issuing another full-register image in production.
    [[nodiscard]] bool Start(FF800WriteEnvelope request) {
        auto& state = shared_->state;
        if (!dispatch_ || blocked_ || (expectedGeneration_ && request.generation != *expectedGeneration_) ||
            state.Phase() == FF800ControlWritePhase::kUncertain ||
            state.Phase() == FF800ControlWritePhase::kFailed ||
            !state.Prepare(request.requestId, request.generation, request.words))
            return false;
        if (!state.Begin(request.requestId, request.generation)) return false;
        const std::weak_ptr<Shared> weak(shared_);
        dispatch_(request, [weak, id = request.requestId, gen = request.generation](bool accepted) {
            if (const auto shared = weak.lock()) {
                // Production callbacks must be serialized onto the controller
                // queue before accessing this state. Tests invoke sequentially.
                if (!shared->state.TransportComplete(id, gen, accepted)) return;
                if (accepted) {
                    // No full configuration readback is supported yet.
                    // Leave the result uncertain; never report verified.
                    (void)shared->state.MarkUnverifiable(id, gen);
                }
            }
        });
        return true;
    }

    void Reset(uint32_t generation) {
        shared_->state.Reset(generation);
        expectedGeneration_ = generation;
        blocked_ = false;
    }

    [[nodiscard]] bool Timeout(uint64_t id, uint32_t generation) {
        const bool changed = shared_->state.Abandon(id, generation, true);
        if (changed) blocked_ = true;
        return changed;
    }

    [[nodiscard]] bool Cancel(uint64_t id, uint32_t generation) {
        const bool changed = shared_->state.Abandon(id, generation, false);
        if (changed) blocked_ = true;
        return changed;
    }

    [[nodiscard]] FF800ControlWritePhase Phase() const noexcept { return shared_->state.Phase(); }
    [[nodiscard]] FF800ControlWriteFailure Failure() const noexcept { return shared_->state.Failure(); }
    [[nodiscard]] std::optional<ConfigWords> VerifiedImage() const noexcept {
        return shared_->state.VerifiedImage();
    }

private:
    struct Shared { FF800ControlWriteState state{}; };
    std::shared_ptr<Shared> shared_;
    Dispatch dispatch_;
    bool blocked_{false};
    std::optional<uint32_t> expectedGeneration_{};
};

} // namespace ASFW::Audio::RME
