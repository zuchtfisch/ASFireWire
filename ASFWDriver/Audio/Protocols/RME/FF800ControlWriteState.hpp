// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// Bookkeeping for a FUTURE FF800 asynchronous register write. No bus I/O.
// A successful FireWire completion is not proof of applied configuration.
// In particular, never infer physical 48 V phantom power from an ACK.
#pragma once

#include "FirefaceSettings.hpp"

#include <cstdint>
#include <optional>

namespace ASFW::Audio::RME {

enum class FF800ControlWritePhase : uint8_t {
    kIdle,
    kPrepared,
    kInFlight,
    kTransportAccepted,
    kVerified,
    kFailed,
    kUncertain
};

enum class FF800ControlWriteFailure : uint8_t {
    kNone,
    kBusy,
    kStaleRequest,
    kTransport,
    kReset,
    kVerificationMismatch,
    kCancelled,
    kTimedOut
};

/// A one-command-at-a-time, generation-scoped protocol state machine.
/// The caller MUST serialize it on the controller queue, and perform any
/// real transport separately. The class deliberately cannot send commands.
class FF800ControlWriteState final {
public:
    [[nodiscard]] FF800ControlWritePhase Phase() const noexcept { return phase_; }
    [[nodiscard]] FF800ControlWriteFailure Failure() const noexcept { return failure_; }
    [[nodiscard]] std::optional<ConfigWords> VerifiedImage() const noexcept { return verified_; }
    [[nodiscard]] std::optional<ConfigWords> Candidate() const noexcept { return candidate_; }
    [[nodiscard]] uint64_t RequestId() const noexcept { return requestId_; }

    /// Returns false while an earlier command is unresolved. No implicit
    /// queueing: otherwise two full register images could overwrite each other.
    [[nodiscard]] bool Prepare(uint64_t requestId, uint32_t generation,
                               ConfigWords image) noexcept {
        if (requestId == 0 || phase_ == FF800ControlWritePhase::kPrepared ||
            phase_ == FF800ControlWritePhase::kInFlight ||
            phase_ == FF800ControlWritePhase::kTransportAccepted)
            return false;
        requestId_ = requestId;
        generation_ = generation;
        candidate_ = image;
        verified_.reset();
        failure_ = FF800ControlWriteFailure::kNone;
        phase_ = FF800ControlWritePhase::kPrepared;
        return true;
    }

    [[nodiscard]] bool Begin(uint64_t requestId, uint32_t generation) noexcept {
        if (!Matches(requestId, generation) || phase_ != FF800ControlWritePhase::kPrepared)
            return false;
        phase_ = FF800ControlWritePhase::kInFlight;
        return true;
    }

    /// The transport has accepted the payload. Do NOT publish a new confirmed
    /// device setting yet; a valid readback must verify it separately.
    [[nodiscard]] bool TransportComplete(uint64_t requestId, uint32_t generation,
                                         bool accepted) noexcept {
        if (!Matches(requestId, generation) || phase_ != FF800ControlWritePhase::kInFlight)
            return false;
        if (accepted) {
            phase_ = FF800ControlWritePhase::kTransportAccepted;
        } else {
            phase_ = FF800ControlWritePhase::kFailed;
            failure_ = FF800ControlWriteFailure::kTransport;
            candidate_.reset();
        }
        return true;
    }

    /// Full config readback, if ever supported by hardware, is necessary to
    /// call this Verified. The existing FF800 status mirror covers only a
    /// subset of bits, so callers must NOT pass that partial status here.
    [[nodiscard]] bool VerifyCompleteImage(uint64_t requestId, uint32_t generation,
                                          ConfigWords observed) noexcept {
        if (!Matches(requestId, generation) || phase_ != FF800ControlWritePhase::kTransportAccepted ||
            !candidate_) return false;
        if (observed == *candidate_) {
            verified_ = observed;
            phase_ = FF800ControlWritePhase::kVerified;
        } else {
            failure_ = FF800ControlWriteFailure::kVerificationMismatch;
            phase_ = FF800ControlWritePhase::kFailed;
        }
        candidate_.reset();
        return true;
    }

    /// No reliable full readback exists: leave the outcome explicitly uncertain.
    [[nodiscard]] bool MarkUnverifiable(uint64_t requestId, uint32_t generation) noexcept {
        if (!Matches(requestId, generation) || phase_ != FF800ControlWritePhase::kTransportAccepted)
            return false;
        phase_ = FF800ControlWritePhase::kUncertain;
        candidate_.reset();
        return true;
    }

    /// Completion may never arrive (timeouts, app disconnect, teardown).
    /// Cancel an unresolved request and prevent a late transport callback from
    /// making it look successful. Timed-out writes are NOT safe to retry
    /// automatically: the hardware might still have accepted the payload.
    [[nodiscard]] bool Abandon(uint64_t requestId, uint32_t generation,
                               bool timedOut) noexcept {
        if (!Matches(requestId, generation) ||
            (phase_ != FF800ControlWritePhase::kPrepared &&
             phase_ != FF800ControlWritePhase::kInFlight &&
             phase_ != FF800ControlWritePhase::kTransportAccepted))
            return false;
        phase_ = FF800ControlWritePhase::kUncertain;
        failure_ = timedOut ? FF800ControlWriteFailure::kTimedOut
                            : FF800ControlWriteFailure::kCancelled;
        candidate_.reset();
        return true;
    }

    /// A bus reset may race an ACK. Discard pending and confirmed state;
    /// late callbacks with the old generation can no longer advance anything.
    void Reset(uint32_t newGeneration) noexcept {
        generation_ = newGeneration;
        requestId_ = 0;
        candidate_.reset();
        verified_.reset();
        phase_ = FF800ControlWritePhase::kIdle;
        failure_ = FF800ControlWriteFailure::kReset;
    }

private:
    [[nodiscard]] bool Matches(uint64_t requestId, uint32_t generation) const noexcept {
        return requestId != 0 && requestId == requestId_ && generation == generation_;
    }

    uint64_t requestId_{0};
    uint32_t generation_{0};
    FF800ControlWritePhase phase_{FF800ControlWritePhase::kIdle};
    FF800ControlWriteFailure failure_{FF800ControlWriteFailure::kNone};
    std::optional<ConfigWords> candidate_{};
    std::optional<ConfigWords> verified_{};
};

} // namespace ASFW::Audio::RME
