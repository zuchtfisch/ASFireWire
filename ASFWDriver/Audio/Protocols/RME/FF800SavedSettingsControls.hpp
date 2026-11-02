// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 ASFireWire Project
//
// Read-only FF800 control projection. The data comes from the flash settings
// cached at the last successful/attempted audio Configure; it is NOT a live
// hardware readback and must never imply phantom voltage was verified.
// No transactions are issued from this class.
#pragma once

#include "../../Controls/IControlProvider.hpp"
#include "FirefaceSettings.hpp"
#include "FirefaceFamilyDriver.hpp"

#include <optional>
#include <utility>

namespace ASFW::Audio::RME {

class FF800SavedSettingsControls final : public Controls::IControlProvider {
public:
    explicit FF800SavedSettingsControls(FirefaceFamilyDriver& driver) noexcept
        : driver_(driver) {}

    [[nodiscard]] std::optional<Controls::ControlSnapshot> CopySnapshot() const override {
        const auto& saved = *driver_.SavedSettingsSlot();
        if (!saved) return std::nullopt;
        namespace AM = AudioModel;
        Controls::ControlSnapshot snapshot{};
        snapshot.family = "rme-ff800-flash";
        snapshot.topology.revision = 1;
        snapshot.state.topologyRevision = 1;
        snapshot.sequence = 0;

        constexpr AM::NodeId node{0xF800};
        snapshot.topology.nodes.push_back({node, "FF800 saved hardware settings (flash)", AM::ProcessorNode{}});

        const FirefaceSettings& s = *saved;
        const auto addBoolean = [&](uint32_t id, const char* name, AM::ParameterSemantic semantic, bool value) {
            snapshot.topology.parameters.push_back({
                AM::ParameterId{id}, AM::ParameterTarget{node}, semantic,
                AM::ParameterDomain{AM::BooleanDomain{}}, name, AM::ParameterAccess::ReadOnly
            });
            snapshot.state.parameters.emplace(AM::ParameterId{id},
                AM::ParameterState{.value = value, .origin = AM::StateOrigin::Cached});
        };
        const auto addEnum = [&](uint32_t id, const char* name, AM::ParameterSemantic semantic,
                                 int64_t value, AM::EnumDomain domain) {
            snapshot.topology.parameters.push_back({
                AM::ParameterId{id}, AM::ParameterTarget{node}, semantic,
                AM::ParameterDomain{std::move(domain)}, name, AM::ParameterAccess::ReadOnly
            });
            snapshot.state.parameters.emplace(AM::ParameterId{id},
                AM::ParameterState{.value = value, .origin = AM::StateOrigin::Cached});
        };
        const AM::EnumDomain jacks{{{0, "Rear"}, {1, "Front"}, {2, "Front + Rear"}}};
        const AM::EnumDomain inputLevels{{{0, "Low Gain"}, {1, "+4 dBu"}, {2, "-10 dBV"}}};
        const AM::EnumDomain outputLevels{{{0, "High Gain"}, {1, "+4 dBu"}, {2, "-10 dBV"}}};

        addBoolean(0xF80001, "Saved phantom power input 7", AM::ParameterSemantic::PhantomPower, s.phantom[0]);
        addBoolean(0xF80002, "Saved phantom power input 8", AM::ParameterSemantic::PhantomPower, s.phantom[1]);
        addBoolean(0xF80003, "Saved phantom power input 9", AM::ParameterSemantic::PhantomPower, s.phantom[2]);
        addBoolean(0xF80004, "Saved phantom power input 10", AM::ParameterSemantic::PhantomPower, s.phantom[3]);
        addEnum(0xF80005, "Saved input 1 socket", AM::ParameterSemantic::Source,
                static_cast<int64_t>(s.ff800Input1), jacks);
        addEnum(0xF80006, "Saved input 7 socket", AM::ParameterSemantic::Source,
                static_cast<int64_t>(s.ff800Input7), jacks);
        addEnum(0xF80007, "Saved input 8 socket", AM::ParameterSemantic::Source,
                static_cast<int64_t>(s.ff800Input8), jacks);
        addEnum(0xF80008, "Saved input reference level", AM::ParameterSemantic::NominalLevel,
                static_cast<int64_t>(s.inputLevel), inputLevels);
        addEnum(0xF80009, "Saved output reference level", AM::ParameterSemantic::NominalLevel,
                static_cast<int64_t>(s.outputLevel), outputLevels);
        return snapshot;
    }

    // WIRING ONLY: validate the semantic write against the family's guarded
    // candidate-image builder. Even a valid candidate is REFUSED. Do not change
    // that until a separate, hardware-tested async write/confirmation path
    // exists. Public snapshot parameters remain ReadOnly, so ControlService
    // rejects normal app writes before this fallback is entered.
    void SetParameter(AudioModel::ParameterId id, const AudioModel::ParameterValue& value,
                      Completion done) override {
        uint64_t kind = 0;
        uint64_t bits = 0;
        if (const auto* boolean = std::get_if<bool>(&value)) {
            kind = 0;
            bits = *boolean ? 1U : 0U;
        } else if (const auto* integer = std::get_if<int64_t>(&value)) {
            kind = 2;
            bits = static_cast<uint64_t>(*integer);
        } else {
            done(kIOReturnBadArgument, false);
            return;
        }
        const auto candidate = driver_.PreviewFF800ControlWrite(id.value, kind, bits);
        if (!candidate) {
            done(candidate.error(), false);
            return;
        }
        // No call to WriteWords / WriteConfig. No state mutation.
        done(kIOReturnNotWritable, false);
    }

private:
    FirefaceFamilyDriver& driver_;
};

} // namespace ASFW::Audio::RME
