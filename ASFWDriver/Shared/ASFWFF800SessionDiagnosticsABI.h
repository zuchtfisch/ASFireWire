// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// FF800 READ-ONLY diagnostic wire contract, v1. Declaration only; no IPC handler.
// This structure NEVER grants authorization to write the FF800 config register.
// In particular, the FF800's cached flash values are not live hardware state.
#ifndef ASFW_FF800_SESSION_DIAGNOSTICS_ABI_H
#define ASFW_FF800_SESSION_DIAGNOSTICS_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASFW_FF800_SESSION_DIAG_ABI_VERSION 1u

// Wire integers, not compiler-dependent C++ enums or Swift Bool.
typedef enum ASFWFF800SessionDiagStatus {
    ASFWFF800SessionDiagUnavailable = 0,
    ASFWFF800SessionDiagValid = 1
} ASFWFF800SessionDiagStatus;

typedef enum ASFWFF800SessionDiagReason {
    ASFWFF800SessionDiagUnknown = 0,
    ASFWFF800SessionDiagEligiblePreviewOnly = 1,
    ASFWFF800SessionDiagDisconnected = 2,
    ASFWFF800SessionDiagRetired = 3,
    ASFWFF800SessionDiagTeardown = 4,
    ASFWFF800SessionDiagHalAttached = 5,
    ASFWFF800SessionDiagStreaming = 6,
    ASFWFF800SessionDiagReconciling = 7,
    ASFWFF800SessionDiagConfigurationRequested = 8,
    ASFWFF800SessionDiagRestartPending = 9,
    ASFWFF800SessionDiagClockChangePending = 10,
    ASFWFF800SessionDiagRouteNotCurrent = 11,
    ASFWFF800SessionDiagGenerationMismatch = 12
} ASFWFF800SessionDiagReason;

// All fields are little-endian integers in a process-local Apple ABI; do not
// cast untrusted bytes to this struct on an architecture with different
// alignment or endianness.
// Version and size must be checked before consuming any further fields.
// A nonzero timestamp is diagnostic metadata only; it cannot establish
// freshness of the route, session, hardware state or their atomicity.
typedef struct ASFWFF800SessionDiagV1 {
    uint32_t abiVersion;
    uint32_t structSize;
    uint32_t status;              // ASFWFF800SessionDiagStatus
    uint32_t reason;              // ASFWFF800SessionDiagReason
    uint64_t deviceGuid;
    uint64_t timestampNs;
    uint32_t cachedGeneration;
    uint32_t routeGeneration;
    uint32_t sessionKnown;        // 0 or 1
    uint32_t routeCurrent;        // 0 or 1
    uint32_t reserved[4];         // must be 0; reserved for a future version
} ASFWFF800SessionDiagV1;

#ifdef __cplusplus
}
#endif

#endif // ASFW_FF800_SESSION_DIAGNOSTICS_ABI_H
