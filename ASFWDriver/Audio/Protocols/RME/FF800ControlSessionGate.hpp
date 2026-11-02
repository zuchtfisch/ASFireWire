// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// PURE POLICY ONLY: conservatively deny future FF800 control-write admission
// unless the audio session is fully idle and no reconfiguration is pending.
// This does not schedule, serialize, or perform FireWire I/O. In particular,
// a session snapshot is not atomic with a later transport write; production
// integration MUST coordinate with the audio session reconcile owner.
#pragma once

#include <cstdint>

namespace ASFW::Audio::RME {

struct FF800ControlSessionConditions {
    bool connected{false};
    bool retired{true};
    bool teardown{true};
    bool halAttached{false};
    bool streaming{false};
    bool reconciling{false};
    bool configurationRequested{false};
    bool restartPending{false};
    bool clockChangePending{false};
    bool routeCurrent{false};
    uint32_t cachedGeneration{0};
    uint32_t routeGeneration{0};
};

/// The first blocking reason in conservative priority order. "Eligible" means
/// only that this momentary read-only preview found no blocker; never that a
/// hardware write is authorized. The conditions may already be stale.
enum class FF800PreviewBlockReason : uint8_t {
    kEligible,
    kDisconnected,
    kRetired,
    kTeardown,
    kHalAttached,
    kStreaming,
    kReconciling,
    kConfigurationRequested,
    kRestartPending,
    kClockChangePending,
    kRouteNotCurrent,
    kGenerationMismatch,
};

[[nodiscard]] constexpr FF800PreviewBlockReason
ExplainFF800ControlSession(const FF800ControlSessionConditions& s) noexcept {
    if (!s.connected) return FF800PreviewBlockReason::kDisconnected;
    if (s.retired) return FF800PreviewBlockReason::kRetired;
    if (s.teardown) return FF800PreviewBlockReason::kTeardown;
    if (s.halAttached) return FF800PreviewBlockReason::kHalAttached;
    if (s.streaming) return FF800PreviewBlockReason::kStreaming;
    if (s.reconciling) return FF800PreviewBlockReason::kReconciling;
    if (s.configurationRequested) return FF800PreviewBlockReason::kConfigurationRequested;
    if (s.restartPending) return FF800PreviewBlockReason::kRestartPending;
    if (s.clockChangePending) return FF800PreviewBlockReason::kClockChangePending;
    if (!s.routeCurrent) return FF800PreviewBlockReason::kRouteNotCurrent;
    if (s.cachedGeneration != s.routeGeneration) return FF800PreviewBlockReason::kGenerationMismatch;
    return FF800PreviewBlockReason::kEligible;
}

/// Stable diagnostic identifiers; no I/O, allocation or UI dependency.
[[nodiscard]] constexpr const char*
FF800PreviewBlockReasonName(FF800PreviewBlockReason reason) noexcept {
    switch (reason) {
    case FF800PreviewBlockReason::kEligible: return "eligible-preview-only";
    case FF800PreviewBlockReason::kDisconnected: return "disconnected";
    case FF800PreviewBlockReason::kRetired: return "retired";
    case FF800PreviewBlockReason::kTeardown: return "teardown";
    case FF800PreviewBlockReason::kHalAttached: return "hal-attached";
    case FF800PreviewBlockReason::kStreaming: return "streaming";
    case FF800PreviewBlockReason::kReconciling: return "reconciling";
    case FF800PreviewBlockReason::kConfigurationRequested: return "configuration-requested";
    case FF800PreviewBlockReason::kRestartPending: return "restart-pending";
    case FF800PreviewBlockReason::kClockChangePending: return "clock-change-pending";
    case FF800PreviewBlockReason::kRouteNotCurrent: return "route-not-current";
    case FF800PreviewBlockReason::kGenerationMismatch: return "generation-mismatch";
    }
    return "unknown";
}

/// A conservative eligibility preview, never authorization to write hardware.
/// Even true is insufficient without an exclusive device configuration lease
/// and full-image hardware verification, neither of which exists here.
[[nodiscard]] constexpr bool
CanPreviewFF800ControlSession(const FF800ControlSessionConditions& session) noexcept {
    return ExplainFF800ControlSession(session) == FF800PreviewBlockReason::kEligible;
}

} // namespace ASFW::Audio::RME
