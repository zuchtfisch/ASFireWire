// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// Pure wire mapping and validation; neither a UserClient handler nor a
// controller transaction. No FF800 control-write path is enabled.
#pragma once

#include "../../../Shared/ASFWFF800SessionDiagnosticsABI.h"
#include "FF800ControlSessionDiagnostics.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ASFW::Audio::RME {

static_assert(std::is_standard_layout_v<ASFWFF800SessionDiagV1>);
static_assert(std::is_trivially_copyable_v<ASFWFF800SessionDiagV1>);
static_assert(sizeof(ASFWFF800SessionDiagV1) == 64);
static_assert(offsetof(ASFWFF800SessionDiagV1, deviceGuid) == 16);

[[nodiscard]] constexpr uint32_t
FF800PreviewReasonWireValue(FF800PreviewBlockReason reason) noexcept {
    switch (reason) {
    case FF800PreviewBlockReason::kEligible: return ASFWFF800SessionDiagEligiblePreviewOnly;
    case FF800PreviewBlockReason::kDisconnected: return ASFWFF800SessionDiagDisconnected;
    case FF800PreviewBlockReason::kRetired: return ASFWFF800SessionDiagRetired;
    case FF800PreviewBlockReason::kTeardown: return ASFWFF800SessionDiagTeardown;
    case FF800PreviewBlockReason::kHalAttached: return ASFWFF800SessionDiagHalAttached;
    case FF800PreviewBlockReason::kStreaming: return ASFWFF800SessionDiagStreaming;
    case FF800PreviewBlockReason::kReconciling: return ASFWFF800SessionDiagReconciling;
    case FF800PreviewBlockReason::kConfigurationRequested: return ASFWFF800SessionDiagConfigurationRequested;
    case FF800PreviewBlockReason::kRestartPending: return ASFWFF800SessionDiagRestartPending;
    case FF800PreviewBlockReason::kClockChangePending: return ASFWFF800SessionDiagClockChangePending;
    case FF800PreviewBlockReason::kRouteNotCurrent: return ASFWFF800SessionDiagRouteNotCurrent;
    case FF800PreviewBlockReason::kGenerationMismatch: return ASFWFF800SessionDiagGenerationMismatch;
    }
    return ASFWFF800SessionDiagUnknown;
}

[[nodiscard]] constexpr ASFWFF800SessionDiagV1
FF800UnavailableDiagnosticV1() noexcept {
    ASFWFF800SessionDiagV1 result{};
    result.abiVersion = ASFW_FF800_SESSION_DIAG_ABI_VERSION;
    result.structSize = sizeof(result);
    result.status = ASFWFF800SessionDiagUnavailable;
    result.reason = ASFWFF800SessionDiagUnknown;
    return result;
}

/// Builds a diagnostic value from explicitly supplied observations. No bus IO.
/// An unknown session is UNAVAILABLE, not "disconnected" or "eligible".
[[nodiscard]] inline ASFWFF800SessionDiagV1
BuildFF800DiagnosticV1(
    uint64_t deviceGuid, uint64_t timestampNs,
    const std::optional<Session::SessionCoordinationSnapshot>& session,
    FF800ControlRouteDiagnostics route) noexcept {
    auto result = FF800UnavailableDiagnosticV1();
    if (deviceGuid == 0 || timestampNs == 0 || !session) return result;
    const auto explanation = ExplainFF800PreviewFromSnapshot(session, route);
    result.status = ASFWFF800SessionDiagValid;
    result.reason = FF800PreviewReasonWireValue(explanation.reason);
    result.deviceGuid = deviceGuid;
    result.timestampNs = timestampNs;
    result.cachedGeneration = route.cachedGeneration;
    result.routeGeneration = route.routeGeneration;
    result.sessionKnown = explanation.sessionKnown ? 1u : 0u;
    result.routeCurrent = route.routeCurrent ? 1u : 0u;
    return result;
}

/// Structural validation only. Preserve a valid *diagnostic* even when
/// the route is stale so the UI can explain why it was rejected. This does
/// not prove freshness, authorization, or actual FF800 hardware state.
[[nodiscard]] constexpr bool
ValidFF800DiagnosticV1(const ASFWFF800SessionDiagV1& value,
                       uint64_t expectedGuid) noexcept {
    if (expectedGuid == 0 || value.abiVersion != ASFW_FF800_SESSION_DIAG_ABI_VERSION ||
        value.structSize != sizeof(value) ||
        value.status != ASFWFF800SessionDiagValid ||
        value.deviceGuid != expectedGuid || value.timestampNs == 0 ||
        value.sessionKnown != 1 || value.routeCurrent > 1 ||
        value.reason < ASFWFF800SessionDiagEligiblePreviewOnly ||
        value.reason > ASFWFF800SessionDiagGenerationMismatch) {
        return false;
    }
    for (uint32_t field : value.reserved) {
        if (field != 0) return false;
    }
    return true;
}

/// Independent generation check for freshness *classification*, not write
/// authorization. A matching integer alone cannot rule out a bus-reset race.
[[nodiscard]] constexpr bool
FF800DiagnosticRouteMatchesGeneration(const ASFWFF800SessionDiagV1& value,
                                      uint64_t expectedGuid,
                                      uint32_t expectedGeneration) noexcept {
    return ValidFF800DiagnosticV1(value, expectedGuid) &&
           value.routeCurrent == 1 &&
           value.cachedGeneration == expectedGeneration &&
           value.routeGeneration == expectedGeneration;
}

} // namespace ASFW::Audio::RME
