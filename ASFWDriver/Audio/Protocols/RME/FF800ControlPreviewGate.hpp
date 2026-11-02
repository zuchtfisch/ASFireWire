// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// Pure safety gate for FF800 control-write previews. No bus transactions.
// A bus reset invalidates the cache; flash alone is not an applied baseline.
#pragma once

#include "FF800ControlWriteState.hpp"
#include "FirefaceSettings.hpp"

#include <optional>

namespace ASFW::Audio::RME {

[[nodiscard]] inline bool HasFF800ControlPreviewBaseline(
    FirefaceModel model,
    FF800ControlWritePhase phase,
    const std::optional<FirefaceSettings>& saved,
    const std::optional<ConfigWords>& written) noexcept {
    if (model != FirefaceModel::kFF800 ||
        phase != FF800ControlWritePhase::kIdle ||
        !saved || !written) return false;

    FirefaceSettings basis = *saved;
    basis.clockMaster = true; // Existing Configure/WriteConfig policy.
    return EncodeConfig(model, basis) == *written;
}

} // namespace ASFW::Audio::RME
