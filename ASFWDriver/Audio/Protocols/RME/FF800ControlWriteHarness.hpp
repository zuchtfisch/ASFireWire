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
    using ControllerTask = std::function<void()>;
    using PostToController = std::function<void(ControllerTask)>;

    explicit FF800ControlWriteHarness(Dispatch dispatch,
                                       PostToController post = {})
        : shared_(std::make_shared<Shared>()), dispatch_(std::move(dispatch)),
          post_(std::move(post)) {}

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
        const uint64_t epoch = shared_->epoch;
        // A real transport can complete on an arbitrary callback thread.
        // The injected PostToController is responsible for serializing that
        // completion with Start/Reset/Timeout/Cancel on the controller queue.
        // The default inline path is ONLY for existing single-threaded tests.
        const auto post = post_;
        dispatch_(request, [weak, epoch, id = request.requestId, gen = request.generation,
                            post](bool accepted) {
            ControllerTask deliver = [weak, epoch, id, gen, accepted] {
                if (const auto shared = weak.lock()) {
                    if (shared->epoch != epoch) return;
                    if (!shared->state.TransportComplete(id, gen, accepted)) return;
                    if (accepted) {
                        // Transport ACK never proves the physical setting.
                        (void)shared->state.MarkUnverifiable(id, gen);
                    }
                }
            };
            if (post) post(std::move(deliver));
            else deliver();
        });
        return true;
    }

    void Reset(uint32_t generation) {
        ++shared_->epoch; // Invalidate callbacks even if ids/generation are reused.
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
    struct Shared {
        FF800ControlWriteState state{};
        uint64_t epoch{0};
    };
    std::shared_ptr<Shared> shared_;
    Dispatch dispatch_;
    PostToController post_;
    bool blocked_{false};
    std::optional<uint32_t> expectedGeneration_{};
};

} // namespace ASFW::Audio::RME
