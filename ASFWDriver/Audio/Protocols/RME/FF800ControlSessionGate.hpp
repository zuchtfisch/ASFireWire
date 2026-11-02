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
    bool restartPending{false};
    bool clockChangePending{false};
    bool routeCurrent{false};
    uint32_t cachedGeneration{0};
    uint32_t routeGeneration{0};
};

/// A conservative eligibility preview, never authorization to write hardware.
/// Even true is insufficient without an exclusive device configuration lease
/// and full-image hardware verification, neither of which exists here.
[[nodiscard]] constexpr bool
CanPreviewFF800ControlSession(const FF800ControlSessionConditions& session) noexcept {
    return session.connected && !session.retired && !session.teardown &&
           !session.halAttached && !session.streaming &&
           !session.reconciling && !session.restartPending &&
           !session.clockChangePending && session.routeCurrent &&
           session.cachedGeneration == session.routeGeneration;
}

} // namespace ASFW::Audio::RME
