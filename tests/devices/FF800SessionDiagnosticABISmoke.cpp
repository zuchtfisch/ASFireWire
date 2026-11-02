// SPDX-License-Identifier: Apache-2.0
// Host-only ABI smoke test. No DriverKit, Apple framework, bus access or device.
#include "../../ASFWDriver/Shared/ASFWFF800SessionDiagnosticsABI.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(ASFW_FF800_SESSION_DIAG_ABI_VERSION == 1u);
static_assert(std::is_standard_layout_v<ASFWFF800SessionDiagV1>);
static_assert(std::is_trivially_copyable_v<ASFWFF800SessionDiagV1>);
static_assert(sizeof(ASFWFF800SessionDiagV1) == 64u);
static_assert(alignof(ASFWFF800SessionDiagV1) == alignof(uint64_t));
static_assert(offsetof(ASFWFF800SessionDiagV1, abiVersion) == 0u);
static_assert(offsetof(ASFWFF800SessionDiagV1, structSize) == 4u);
static_assert(offsetof(ASFWFF800SessionDiagV1, status) == 8u);
static_assert(offsetof(ASFWFF800SessionDiagV1, reason) == 12u);
static_assert(offsetof(ASFWFF800SessionDiagV1, deviceGuid) == 16u);
static_assert(offsetof(ASFWFF800SessionDiagV1, timestampNs) == 24u);
static_assert(offsetof(ASFWFF800SessionDiagV1, cachedGeneration) == 32u);
static_assert(offsetof(ASFWFF800SessionDiagV1, routeGeneration) == 36u);
static_assert(offsetof(ASFWFF800SessionDiagV1, sessionKnown) == 40u);
static_assert(offsetof(ASFWFF800SessionDiagV1, routeCurrent) == 44u);
static_assert(offsetof(ASFWFF800SessionDiagV1, reserved) == 48u);
static_assert(ASFWFF800SessionDiagUnavailable == 0);
static_assert(ASFWFF800SessionDiagValid == 1);
static_assert(ASFWFF800SessionDiagEligiblePreviewOnly == 1);
static_assert(ASFWFF800SessionDiagGenerationMismatch == 12);

int main() {
    // Document the intentionally inert, zero-initialized "unavailable" state.
    ASFWFF800SessionDiagV1 sample{};
    sample.abiVersion = ASFW_FF800_SESSION_DIAG_ABI_VERSION;
    sample.structSize = sizeof(sample);
    return sample.status == ASFWFF800SessionDiagUnavailable &&
                   sample.deviceGuid == 0 &&
                   sample.sessionKnown == 0 &&
                   sample.routeCurrent == 0 &&
                   sample.reserved[0] == 0
               ? 0
               : 1;
}
