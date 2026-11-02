// SPDX-License-Identifier: Apache-2.0
#include "Audio/Protocols/RME/FF800ControlSettings.hpp"
#include <gtest/gtest.h>

namespace {
using namespace ASFW::Audio::RME;

TEST(FF800ControlSettingsTests, PhantomChangesOnlyRequestedChannel) {
    const FirefaceSettings original{};
    for (unsigned i = 0; i < 4; ++i) {
        const auto selected = static_cast<FF800Setting>(
            static_cast<unsigned>(FF800Setting::kPhantom7) + i);
        const auto changed = WithFF800Setting(FirefaceModel::kFF800, original, selected, 1);
        ASSERT_TRUE(changed.has_value());
        for (unsigned j = 0; j < 4; ++j) EXPECT_EQ(changed->phantom[j], i == j);
    }
}

TEST(FF800ControlSettingsTests, InvalidInputLeavesSourceUntouched) {
    const FirefaceSettings original{};
    EXPECT_EQ(WithFF800Setting(FirefaceModel::kFF800, original,
                              FF800Setting::kPhantom7, 2).error(),
              FF800SettingError::kInvalidValue);
    EXPECT_EQ(WithFF800Setting(FirefaceModel::kFF800, original,
                              FF800Setting::kInput1Jack, 3).error(),
              FF800SettingError::kInvalidValue);
    EXPECT_EQ(WithFF800Setting(FirefaceModel::kFF800, original,
                              FF800Setting::kOutputLevel, 3).error(),
              FF800SettingError::kInvalidValue);
    EXPECT_FALSE(original.phantom[0]);
}

TEST(FF800ControlSettingsTests, FF400CannotReceiveFF800Changes) {
    const FirefaceSettings original{};
    auto changed = WithFF800Setting(FirefaceModel::kFF400, original,
                                    FF800Setting::kPhantom7, 1);
    ASSERT_FALSE(changed.has_value());
    EXPECT_EQ(changed.error(), FF800SettingError::kUnsupportedModel);
}

TEST(FF800ControlSettingsTests, EncodedConfigPreservesUnrelatedWords) {
    FirefaceSettings original{};
    original.clockMaster = true;
    original.ff800Input1 = JackSelect::kRear;
    const auto baseline = EncodeConfig(FirefaceModel::kFF800, original);
    const auto edited = EncodeFF800SettingChange(FirefaceModel::kFF800, original,
                                                  FF800Setting::kPhantom7, 1);
    ASSERT_TRUE(edited.has_value());
    EXPECT_EQ((*edited)[0], baseline[0] | 0x1U);
    EXPECT_EQ((*edited)[1], baseline[1]);
    EXPECT_EQ((*edited)[2], baseline[2]);
}
TEST(FF800ControlSettingsTests, ControlIPCRejectsUnknownIdsAndKinds) {
    const FirefaceSettings original{};
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                 0xF80000, 0, 1).error(), FF800SettingError::kUnknownParameter);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                 0xF8000A, 2, 1).error(), FF800SettingError::kUnknownParameter);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                 0xF80001, 2, 1).error(), FF800SettingError::kInvalidValue);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                 0xF80005, 0, 1).error(), FF800SettingError::kInvalidValue);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                 0xF80001, 0, 2).error(), FF800SettingError::kInvalidValue);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                 0xF80008, 2, UINT64_MAX).error(), FF800SettingError::kInvalidValue);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF400, original,
                 0xF80005, 2, 1).error(), FF800SettingError::kUnsupportedModel);
}

TEST(FF800ControlSettingsTests, ControlIPCKnownValueProducesExpectedImage) {
    FirefaceSettings original{};
    const auto encoded = EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
                                                      0xF80006, 2, 0);
    const auto expected = EncodeFF800SettingChange(FirefaceModel::kFF800, original,
                                                    FF800Setting::kInput7Jack, 0);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(*encoded, *expected);
    EXPECT_EQ(EncodeFF800ControlParameter(FirefaceModel::kFF800, original,
              0xF80001, 0, 1).value(),
              EncodeFF800SettingChange(FirefaceModel::kFF800, original,
              FF800Setting::kPhantom7, 1).value());
}

} // namespace
