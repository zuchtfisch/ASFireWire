// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// FF800 control configuration image for the existing WriteBlockLEQuadlets
// transport. A pure serializer: no bus access, no device-state mutation.
#pragma once

#include "FirefaceRegisters.hpp"
#include "FirefaceSettings.hpp"

#include <array>
#include <cstdint>

namespace ASFW::Audio::RME {

struct FF800ControlWireImage {
    static constexpr uint64_t kAddress = Register::kFF800Config;
    static constexpr uint32_t kByteCount = 12;

    uint64_t address{kAddress};
    std::array<uint8_t, kByteCount> payload{};

    [[nodiscard]] bool operator==(const FF800ControlWireImage&) const = default;
};

/// Exactly one three-quadlet WRITE BLOCK to the FF800 write-only config
/// register. The payload is little-endian, unlike generic AV/C quadlets.
/// The hardware's ACK cannot be interpreted as settings readback.
[[nodiscard]] constexpr FF800ControlWireImage
MakeFF800ControlWireImage(const ConfigWords& words) noexcept {
    FF800ControlWireImage image{};
    for (uint32_t i = 0; i < words.size(); ++i) {
        const uint32_t value = words[i];
        image.payload[i * 4] = static_cast<uint8_t>(value);
        image.payload[i * 4 + 1] = static_cast<uint8_t>(value >> 8);
        image.payload[i * 4 + 2] = static_cast<uint8_t>(value >> 16);
        image.payload[i * 4 + 3] = static_cast<uint8_t>(value >> 24);
    }
    return image;
}

} // namespace ASFW::Audio::RME
