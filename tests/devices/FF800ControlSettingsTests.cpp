// SPDX-License-Identifier: Apache-2.0
#include "Audio/Protocols/RME/FF800ControlSettings.hpp"
#include "Audio/Protocols/RME/FF800ControlWriteState.hpp"
#include "Audio/Protocols/RME/FF800ControlWriteHarness.hpp"
#include "Audio/Protocols/RME/FF800ControlWireImage.hpp"
#include "Audio/Protocols/RME/FF800ControlPreviewGate.hpp"
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

TEST(FF800WriteStateTests, TransportAckIsNotHardwareConfirmation) {
    FF800ControlWriteState state;
    const ConfigWords candidate{1, 2, 3};
    ASSERT_TRUE(state.Prepare(100, 5, candidate));
    EXPECT_FALSE(state.Prepare(101, 5, candidate)); // one unresolved request
    ASSERT_TRUE(state.Begin(100, 5));
    ASSERT_TRUE(state.TransportComplete(100, 5, true));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kTransportAccepted);
    EXPECT_FALSE(state.VerifiedImage().has_value());
    ASSERT_TRUE(state.MarkUnverifiable(100, 5));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kUncertain);
    EXPECT_FALSE(state.VerifiedImage().has_value());
}

TEST(FF800WriteStateTests, LateCompletionAfterResetIsIgnored) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(102, 5, ConfigWords{1, 2, 3}));
    ASSERT_TRUE(state.Begin(102, 5));
    state.Reset(6);
    EXPECT_FALSE(state.TransportComplete(102, 5, true));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kIdle);
    EXPECT_FALSE(state.Candidate().has_value());
    EXPECT_FALSE(state.VerifiedImage().has_value());
}

TEST(FF800WriteStateTests, OnlyCompleteMatchingReadbackIsVerified) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(103, 5, ConfigWords{1, 2, 3}));
    ASSERT_TRUE(state.Begin(103, 5));
    ASSERT_TRUE(state.TransportComplete(103, 5, true));
    ASSERT_TRUE(state.VerifyCompleteImage(103, 5, ConfigWords{1, 2, 4}));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kFailed);
    EXPECT_FALSE(state.VerifiedImage().has_value());
    ASSERT_TRUE(state.Prepare(104, 5, ConfigWords{5, 6, 7}));
    ASSERT_TRUE(state.Begin(104, 5));
    ASSERT_TRUE(state.TransportComplete(104, 5, true));
    ASSERT_TRUE(state.VerifyCompleteImage(104, 5, ConfigWords{5, 6, 7}));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kVerified);
    ASSERT_TRUE(state.VerifiedImage().has_value());
    EXPECT_EQ(*state.VerifiedImage(), (ConfigWords{5, 6, 7}));
}

TEST(FF800WriteStateTests, TransportFailureDoesNotUpdateState) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(105, 9, ConfigWords{1, 2, 3}));
    ASSERT_TRUE(state.Begin(105, 9));
    ASSERT_TRUE(state.TransportComplete(105, 9, false));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kFailed);
    EXPECT_EQ(state.Failure(), FF800ControlWriteFailure::kTransport);
    EXPECT_FALSE(state.VerifiedImage().has_value());
}

TEST(FF800WriteStateTests, TimeoutCannotBecomeSuccessFromLateCallback) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(201, 10, ConfigWords{1, 2, 3}));
    ASSERT_TRUE(state.Begin(201, 10));
    ASSERT_TRUE(state.Abandon(201, 10, true));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kUncertain);
    EXPECT_EQ(state.Failure(), FF800ControlWriteFailure::kTimedOut);
    EXPECT_FALSE(state.TransportComplete(201, 10, true));
    EXPECT_FALSE(state.VerifyCompleteImage(201, 10, ConfigWords{1, 2, 3}));
    EXPECT_FALSE(state.VerifiedImage().has_value());
}

TEST(FF800WriteStateTests, CancelledPreparedRequestCannotBeSent) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(202, 10, ConfigWords{1, 2, 3}));
    ASSERT_TRUE(state.Abandon(202, 10, false));
    EXPECT_EQ(state.Failure(), FF800ControlWriteFailure::kCancelled);
    EXPECT_FALSE(state.Begin(202, 10));
    EXPECT_FALSE(state.VerifiedImage().has_value());
}

TEST(FF800TransportHarnessTests, AcceptedTransportIsNotVerifiedAndCannotAutoRetry) {
    FF800ControlWriteHarness::Completion complete;
    unsigned sends = 0;
    FF800ControlWriteHarness harness([&](const FF800WriteEnvelope& request,
                                         FF800ControlWriteHarness::Completion callback) {
        ++sends;
        EXPECT_EQ(request.words, (ConfigWords{1, 2, 3}));
        complete = std::move(callback);
    });
    ASSERT_TRUE(harness.Start({301, 4, {1, 2, 3}}));
    EXPECT_FALSE(harness.Start({302, 4, {4, 5, 6}}));
    ASSERT_TRUE(static_cast<bool>(complete));
    complete(true);
    EXPECT_EQ(harness.Phase(), FF800ControlWritePhase::kUncertain);
    EXPECT_FALSE(harness.VerifiedImage().has_value());
    EXPECT_FALSE(harness.Start({303, 4, {4, 5, 6}}));
    EXPECT_EQ(sends, 1U);
}

TEST(FF800TransportHarnessTests, TimedOutLateCompletionNeverVerifiesOrRetries) {
    FF800ControlWriteHarness::Completion complete;
    unsigned sends = 0;
    FF800ControlWriteHarness harness([&](const FF800WriteEnvelope&,
                                         FF800ControlWriteHarness::Completion callback) {
        ++sends;
        complete = std::move(callback);
    });
    ASSERT_TRUE(harness.Start({304, 8, {1, 2, 3}}));
    ASSERT_TRUE(harness.Timeout(304, 8));
    complete(true);
    EXPECT_EQ(harness.Failure(), FF800ControlWriteFailure::kTimedOut);
    EXPECT_EQ(harness.Phase(), FF800ControlWritePhase::kUncertain);
    EXPECT_FALSE(harness.Start({305, 8, {4, 5, 6}}));
    EXPECT_EQ(sends, 1U);
}

TEST(FF800TransportHarnessTests, ResetRejectsOldCallbackAndAllowsNewGeneration) {
    FF800ControlWriteHarness::Completion stale;
    FF800ControlWriteHarness harness([&](const FF800WriteEnvelope&,
                                         FF800ControlWriteHarness::Completion callback) {
        stale = std::move(callback);
    });
    ASSERT_TRUE(harness.Start({306, 11, {1, 2, 3}}));
    const auto old = stale;
    harness.Reset(12);
    old(true);
    EXPECT_EQ(harness.Phase(), FF800ControlWritePhase::kIdle);
    EXPECT_FALSE(harness.VerifiedImage().has_value());
    EXPECT_TRUE(harness.Start({307, 12, {4, 5, 6}}));
}

TEST(FF800TransportHarnessTests, DestroyedHarnessIgnoresLateCallback) {
    FF800ControlWriteHarness::Completion complete;
    {
        FF800ControlWriteHarness harness([&](const FF800WriteEnvelope&,
                                             FF800ControlWriteHarness::Completion callback) {
            complete = std::move(callback);
        });
        ASSERT_TRUE(harness.Start({308, 2, {1, 2, 3}}));
    }
    ASSERT_TRUE(static_cast<bool>(complete));
    complete(true); // weak ownership: no dangling pointer to the harness
    SUCCEED();
}

TEST(FF800TransportHarnessTests, RejectedTransportCannotBeMisreportedVerified) {
    FF800ControlWriteHarness harness([](const FF800WriteEnvelope&,
                                        FF800ControlWriteHarness::Completion callback) {
        callback(false);
    });
    ASSERT_TRUE(harness.Start({309, 2, {1, 2, 3}}));
    EXPECT_EQ(harness.Phase(), FF800ControlWritePhase::kFailed);
    EXPECT_EQ(harness.Failure(), FF800ControlWriteFailure::kTransport);
    EXPECT_FALSE(harness.VerifiedImage().has_value());
    EXPECT_FALSE(harness.Start({310, 2, {4, 5, 6}}));
}

TEST(FF800ControlWireImageTests, UsesExactFF800ConfigurationRegisterAndTwelveBytes) {
    constexpr auto image = MakeFF800ControlWireImage({0, 0, 0});
    static_assert(FF800ControlWireImage::kByteCount == 12);
    EXPECT_EQ(image.address, Register::kFF800Config);
    EXPECT_EQ(image.address, 0x0000fc88f014ULL);
    EXPECT_EQ(image.payload.size(), 12U);
}

TEST(FF800ControlWireImageTests, SerializesAllThreeWordsLittleEndian) {
    const auto image = MakeFF800ControlWireImage({0x12345678, 0x90abcdef, 0x01020304});
    const std::array<uint8_t, 12> expected{
        0x78, 0x56, 0x34, 0x12,
        0xef, 0xcd, 0xab, 0x90,
        0x04, 0x03, 0x02, 0x01
    };
    EXPECT_EQ(image.payload, expected);
}

TEST(FF800ControlWireImageTests, ProducesImageFromExistingSemanticSettingsEncoder) {
    FirefaceSettings saved{};
    saved.clockMaster = true;
    const auto candidate = EncodeFF800ControlParameter(
        FirefaceModel::kFF800, saved, 0xF80006, 2, 0);
    ASSERT_TRUE(candidate.has_value());
    const auto image = MakeFF800ControlWireImage(*candidate);
    for (size_t i = 0; i < candidate->size(); ++i) {
        uint32_t assembled = 0;
        for (size_t byte = 0; byte < 4; ++byte)
            assembled |= static_cast<uint32_t>(image.payload[i * 4 + byte]) << (8 * byte);
        EXPECT_EQ(assembled, (*candidate)[i]);
    }
}

TEST(FF800TransportHarnessTests, RefusesPreviousGenerationAfterReset) {
    unsigned sends = 0;
    FF800ControlWriteHarness harness([&](const FF800WriteEnvelope&,
                                         FF800ControlWriteHarness::Completion) { ++sends; });
    harness.Reset(17);
    EXPECT_FALSE(harness.Start({401, 16, {1, 2, 3}}));
    EXPECT_EQ(sends, 0U);
    EXPECT_TRUE(harness.Start({402, 17, {1, 2, 3}}));
    EXPECT_EQ(sends, 1U);
}

TEST(FF800WriteStateTests, UnverifiableWriteBlocksNewPreparationUntilReset) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(501, 7, {1, 2, 3}));
    ASSERT_TRUE(state.Begin(501, 7));
    ASSERT_TRUE(state.TransportComplete(501, 7, true));
    ASSERT_TRUE(state.MarkUnverifiable(501, 7));
    EXPECT_FALSE(state.Prepare(502, 7, {4, 5, 6}));
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kUncertain);
    EXPECT_EQ(state.RequestId(), 501U);
    state.Reset(8);
    EXPECT_TRUE(state.Prepare(502, 8, {4, 5, 6}));
}

TEST(FF800WriteStateTests, TimeoutBlocksNewPreparationAndLateAck) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(503, 7, {1, 2, 3}));
    ASSERT_TRUE(state.Begin(503, 7));
    ASSERT_TRUE(state.Abandon(503, 7, true));
    EXPECT_FALSE(state.Prepare(504, 7, {4, 5, 6}));
    EXPECT_FALSE(state.TransportComplete(503, 7, true));
    EXPECT_EQ(state.Failure(), FF800ControlWriteFailure::kTimedOut);
    EXPECT_FALSE(state.VerifiedImage().has_value());
}

TEST(FF800WriteStateTests, CancellationBlocksNewPreparation) {
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(505, 7, {1, 2, 3}));
    ASSERT_TRUE(state.Abandon(505, 7, false));
    EXPECT_FALSE(state.Prepare(506, 7, {4, 5, 6}));
    EXPECT_EQ(state.Failure(), FF800ControlWriteFailure::kCancelled);
    EXPECT_EQ(state.Phase(), FF800ControlWritePhase::kUncertain);
}

TEST(FF800ControlPreviewGateTests, RequiresFreshMatchingWrittenBaseline) {
    FirefaceSettings saved{};
    saved.clockMaster = false;
    FirefaceSettings forced = saved;
    forced.clockMaster = true;
    const ConfigWords actual = EncodeConfig(FirefaceModel::kFF800, forced);
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        FF800ControlWritePhase::kIdle, saved, std::nullopt));
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        FF800ControlWritePhase::kIdle, std::nullopt, actual));
    EXPECT_TRUE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        FF800ControlWritePhase::kIdle, saved, actual));
}

TEST(FF800ControlPreviewGateTests, RefusesOtherModelsAndMismatchedImages) {
    FirefaceSettings saved{};
    saved.clockMaster = true;
    const ConfigWords correct = EncodeConfig(FirefaceModel::kFF800, saved);
    ConfigWords incorrect = correct;
    incorrect[1] ^= 0x100U;
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF400,
        FF800ControlWritePhase::kIdle, saved, correct));
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        FF800ControlWritePhase::kIdle, saved, incorrect));
}

TEST(FF800ControlPreviewGateTests, UncertainAndPendingWritesInvalidatePreview) {
    FirefaceSettings saved{};
    saved.clockMaster = true;
    const ConfigWords applied = EncodeConfig(FirefaceModel::kFF800, saved);
    for (const auto phase : {FF800ControlWritePhase::kPrepared,
                             FF800ControlWritePhase::kInFlight,
                             FF800ControlWritePhase::kTransportAccepted,
                             FF800ControlWritePhase::kUncertain,
                             FF800ControlWritePhase::kFailed}) {
        EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
            phase, saved, applied));
    }
}

TEST(FF800ControlPreviewGateTests, ResetRequiresCacheAndWrittenImageReestablished) {
    FirefaceSettings saved{};
    saved.clockMaster = true;
    const ConfigWords written = EncodeConfig(FirefaceModel::kFF800, saved);
    FF800ControlWriteState state;
    ASSERT_TRUE(state.Prepare(601, 12, written));
    ASSERT_TRUE(state.Begin(601, 12));
    ASSERT_TRUE(state.TransportComplete(601, 12, true));
    ASSERT_TRUE(state.MarkUnverifiable(601, 12));
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        state.Phase(), saved, written));
    state.Reset(13);
    // The real driver clears its flash settings and written image on reset.
    std::optional<FirefaceSettings> cached;
    std::optional<ConfigWords> applied;
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        state.Phase(), cached, applied));
    cached = saved;
    EXPECT_FALSE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        state.Phase(), cached, applied));
    applied = written;
    EXPECT_TRUE(HasFF800ControlPreviewBaseline(FirefaceModel::kFF800,
        state.Phase(), cached, applied));
}

} // namespace
