// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// READ-ONLY adapter from the real audio scheduler's diagnostic snapshot to
// the FF800 preview gate. It never provides write authorization, a lease,
// transport ownership, or hardware readback.
#pragma once

#include "FF800ControlSessionGate.hpp"
#include "../../Session/SessionScheduler.hpp"

#include <optional>

namespace ASFW::Audio::RME {

struct FF800ControlRouteDiagnostics {
    bool connected{false};
    bool routeCurrent{false};
    uint32_t cachedGeneration{0};
    uint32_t routeGeneration{0};
};

/// Missing session means no known idle owner: fail closed. The snapshot and
/// route diagnostics may already be stale when this returns; even an eligible
/// result is NOT permission to dispatch any FireWire register write.
[[nodiscard]] inline FF800ControlSessionConditions
FF800PreviewConditionsFromSnapshot(
    const std::optional<Session::SessionCoordinationSnapshot>& session,
    FF800ControlRouteDiagnostics route) noexcept {
    FF800ControlSessionConditions out{};
    if (!session) return out; // safe defaults: offline/retired/teardown
    out.connected = route.connected;
    out.retired = session->retired;
    out.teardown = session->teardown;
    out.halAttached = session->halAttached;
    out.streaming = session->streaming;
    out.reconciling = session->reconciling;
    out.configurationRequested = session->configurationRequested ||
                                  session->requestedTicket != session->completedTicket;
    out.restartPending = session->restartPending;
    out.clockChangePending = session->clockChangePending;
    out.routeCurrent = route.routeCurrent;
    out.cachedGeneration = route.cachedGeneration;
    out.routeGeneration = route.routeGeneration;
    return out;
}

/// Distinguishes an absent session from disconnected live-route diagnostics.
/// Read-only and fail-closed; kEligible is never permission to write hardware.
struct FF800SnapshotPreviewExplanation {
    bool sessionKnown{false};
    FF800PreviewBlockReason reason{FF800PreviewBlockReason::kDisconnected};

    [[nodiscard]] constexpr bool PreviewEligible() const noexcept {
        return sessionKnown && reason == FF800PreviewBlockReason::kEligible;
    }
};

[[nodiscard]] inline FF800SnapshotPreviewExplanation
ExplainFF800PreviewFromSnapshot(
    const std::optional<Session::SessionCoordinationSnapshot>& session,
    FF800ControlRouteDiagnostics route) noexcept {
    if (!session) return {false, FF800PreviewBlockReason::kDisconnected};
    return {true, ExplainFF800ControlSession(FF800PreviewConditionsFromSnapshot(session, route))};
}

[[nodiscard]] inline bool CanPreviewFF800FromSnapshot(
    const std::optional<Session::SessionCoordinationSnapshot>& session,
    FF800ControlRouteDiagnostics route) noexcept {
    return ExplainFF800PreviewFromSnapshot(session, route).PreviewEligible();
}

} // namespace ASFW::Audio::RME
