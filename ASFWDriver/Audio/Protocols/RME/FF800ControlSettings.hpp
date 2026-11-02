// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// FF800 control changes are staged separately from the flash-backed defaults.
// No FireWire I/O occurs here; the caller must validate the device model,
// serialize changes with stream configuration, and perform a deliberate write.
#pragma once

#include "FirefaceSettings.hpp"

#include <array>
#include <cstdint>
#include <expected>

namespace ASFW::Audio::RME {

enum class FF800Setting : uint8_t {
    kPhantom7, kPhantom8, kPhantom9, kPhantom10,
    kInput1Jack, kInput7Jack, kInput8Jack,
    kInputLevel, kOutputLevel
};

enum class FF800SettingError : uint8_t {
    kInvalidValue,
    kUnsupportedModel,
    kUnknownParameter
};

/// An isolated, testable setting mutation. Values are the underlying enum
/// ordinal for jack/level selections and 0 or 1 for phantom power.
/// Invalid values leave the passed settings unchanged.
[[nodiscard]] inline std::expected<FirefaceSettings, FF800SettingError>
WithFF800Setting(FirefaceModel model, const FirefaceSettings& initial,
                 FF800Setting setting, uint32_t value) noexcept {
    if (model != FirefaceModel::kFF800)
        return std::unexpected(FF800SettingError::kUnsupportedModel);

    FirefaceSettings changed = initial;
    switch (setting) {
    case FF800Setting::kPhantom7:
    case FF800Setting::kPhantom8:
    case FF800Setting::kPhantom9:
    case FF800Setting::kPhantom10: {
        if (value > 1) return std::unexpected(FF800SettingError::kInvalidValue);
        const auto index = static_cast<uint8_t>(setting) -
                           static_cast<uint8_t>(FF800Setting::kPhantom7);
        changed.phantom[index] = (value == 1);
        break;
    }
    case FF800Setting::kInput1Jack:
    case FF800Setting::kInput7Jack:
    case FF800Setting::kInput8Jack: {
        if (value > static_cast<uint32_t>(JackSelect::kFrontAndRear))
            return std::unexpected(FF800SettingError::kInvalidValue);
        const auto jack = static_cast<JackSelect>(value);
        if (setting == FF800Setting::kInput1Jack) changed.ff800Input1 = jack;
        else if (setting == FF800Setting::kInput7Jack) changed.ff800Input7 = jack;
        else changed.ff800Input8 = jack;
        break;
    }
    case FF800Setting::kInputLevel:
        if (value > static_cast<uint32_t>(InputLevel::kMinus10dBV))
            return std::unexpected(FF800SettingError::kInvalidValue);
        changed.inputLevel = static_cast<InputLevel>(value);
        break;
    case FF800Setting::kOutputLevel:
        if (value > static_cast<uint32_t>(OutputLevel::kMinus10dBV))
            return std::unexpected(FF800SettingError::kInvalidValue);
        changed.outputLevel = static_cast<OutputLevel>(value);
        break;
    default:
        return std::unexpected(FF800SettingError::kUnknownParameter);
    }
    return changed;
}

/// Produces the entire three-quadlet configuration, preserving fields that
/// are unrelated to the requested setting. Nothing is written to flash.
[[nodiscard]] inline std::expected<ConfigWords, FF800SettingError>
EncodeFF800SettingChange(FirefaceModel model, const FirefaceSettings& initial,
                         FF800Setting setting, uint32_t value) noexcept {
    auto changed = WithFF800Setting(model, initial, setting, value);
    if (!changed) return std::unexpected(changed.error());
    return EncodeConfig(model, *changed);
}

    /// The parameter ids exported through asfw.controls.v1. Unlike an enum
    /// cast, this refuses an unknown id before any configuration is produced.
    /// The returned payload is a complete register image, NOT a flash write.
    [[nodiscard]] inline std::expected<ConfigWords, FF800SettingError>
    EncodeFF800ControlParameter(FirefaceModel model, const FirefaceSettings& initial,
                                uint32_t parameterId, uint64_t valueKind,
                                uint64_t valueBits) noexcept {
        if (model != FirefaceModel::kFF800)
            return std::unexpected(FF800SettingError::kUnsupportedModel);
        if (parameterId < 0xF80001 || parameterId > 0xF80009)
            return std::unexpected(FF800SettingError::kUnknownParameter);
        const bool phantom = parameterId <= 0xF80004;
        // UI transport: bool kind 0 / integer kind 2. No double, action,
        // negative values, or noncanonical booleans are accepted.
        if (valueKind != (phantom ? 0U : 2U) || valueBits > (phantom ? 1U : 2U))
            return std::unexpected(FF800SettingError::kInvalidValue);
        const auto setting = static_cast<FF800Setting>(parameterId - 0xF80001U);
        return EncodeFF800SettingChange(model, initial, setting,
                                        static_cast<uint32_t>(valueBits));
    }

} // namespace ASFW::Audio::RME
