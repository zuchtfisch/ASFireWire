# FF800 read-only diagnostics: maintainer review handoff

**Status:** review proposal only; no pull request or signed distribution was
created by this document.

**Working branch:** `zuchtfisch/ASFireWire:feature/fireface-800-controls`
([browse branch](https://github.com/zuchtfisch/ASFireWire/tree/feature/fireface-800-controls)).

## Goal and explicit non-goals

A Fireface 800 owner using the signed release from asfw.net can access the
device through the current ASFW driver, but needs a safe way to inspect
driver-side FF800 session and route readiness while future Settings and Routing
support is developed.

This first proposed integration is **read-only diagnostic scaffolding**, not
functional hardware Settings or Routing. It does not write RME FF800 registers,
switch phantom power, apply draft settings, alter audio routes, or grant control
write authority. In particular `eligible-preview-only` is not an authorization
to write anything.

## Current tested evidence (reported by local tester, 2026-11-02)

- Signed public ASFW.app: Developer ID Application, Team
  `F6YA6B56LR`, app identifier `net.asfw.app`, embedded
  `com.apple.developer.driverkit.userclient-access = [net.asfw.driver]`.
- Installed `net.asfw.driver` 0.6.0/33 is `activated enabled`; IOKit
  reports an active `ASFWDriver` and an `RME Fireface 800` audio nub.
- The signed release app can communicate with the installed driver and
  produce a standard 1394 diagnostic report. Its report identified driver
  `v0.6.0 (de0cc7e DIRTY @ fix/port-orpheus-core-fixes)`, ABI 11.
  This does **not** show that selector 84 is present.
- The locally compiled prototype app starts but has
  `Identifier=ASFW`, `Signature=adhoc`, `TeamIdentifier=not set`,
  and no readable embedded entitlements. Keychain search:
  `0 valid identities found`.
- macOS explicitly logged
  `ASFWDriverUserClient:UC failed userclient-access check, needed bundle ID net.asfw.driver`
  followed by `entitlements check failed`. Therefore the prototype never
  gets far enough to exercise the FF800 IPC.
- Tester reported `** TEST SUCCEEDED **` for
  `ASFWTests/FF800SessionDiagnosticWireTests` and `[OK] Build Succeeded`
  with one harmless multi-destination warning, at the then-current branch
  commit `2f6759ea`. The later signing-preflight/documentation changes
  have not been independently built on the tester's Mac.

**Do not interpret the successful signed release connection as a live test
of the new IPC or FF800 Settings/Router writes.**

## Smallest sensible review slices

The feature branch is **not** a small PR: a GitHub comparison against `main`
on 2026-11-02 reported **158 commits** ahead and includes other audio,
scheduler, test, and build modifications. Do not merge the whole branch
as the first FF800 diagnostics contribution. Prepare a clean integration
branch from the maintainers' current target and review the following slices
separately; validate dependencies and API compatibility rather than blind
cherry-picking.

### A. Swift-only, signed-app connection/identity preview (no driver change)

- `ASFW/Views/Fireface800SettingsView.swift`:
  local editor drafts stay disabled for hardware writes; read-only
  saved-settings display, selected GUID, explicit diagnostic refresh,
  availability messages.
- `ASFW/ViewModels/DeviceControlsViewModel.swift`:
  FF800 ROM-unit identity gate (not a model-name check), selection/session
  invalidation, no polling; cross-generation/device-change revalidation.
- `ASFW/Models/ControlSnapshot.swift`:
  fail-closed decoder for the diagnostic packet and cached settings validation.
- `ASFW/Views/ModernContentView.swift` and relevant connector interfaces:
  navigation wiring and invalidation on observed topology generation change.
- `ASFWTests/ControlSnapshotTests.swift`: fake driver and diagnostic tests.

The Swift slice currently expects the new selector and therefore requires a
careful compatibility review with an older installed driver. A signed
app-only test can first confirm that the app is **authorized to connect**
to the existing UserClient, and that an unsupported diagnostic is reported
as unavailable. Do not represent such a test as selector-84 success.

### B. Driver-side selector 84 read-only observation (separate review)

- `ASFWDriver/Shared/ASFWFF800SessionDiagnosticsABI.h`:
  fixed 64-byte v1 packet and availability/reason fields.
- `ASFWDriver/UserClient/Core/ASFWDriverUserClient.iig`,
  `ASFWDriver/UserClient/Handlers/ControlsHandler.{hpp,cpp}`:
  two-GUID-scalar selector 84, FF800 identity gating, best-effort
  unavailable/valid wire packet; **no FireWire transaction**.
- `ASFWDriver/Audio/Core/AudioCoordinator.hpp` and scheduler/session
  observation accessors: observe an existing session only, no session
  creation or control-write admission.
- `ASFWDriver/Audio/Protocols/RME/FF800SessionDiagnosticWire.hpp`:
  packet serialization/validation helpers.

Changes to scheduler/session internals need additional maintainer review for
race windows and real-time behavior; the observation does not atomically
pin the route. Any deployment of this slice requires the maintainer's
authorized driver release process, **not** a tester-side unsigned dext swap.

### C. Keep exploratory hardware-write code out of initial integration

The branch also contains FF800 configuration image builders, simulated
leases and write-state harnesses, and host-only tests. They are **not**
production write authorization and have not established safe readback,
write cancellation, or ownership with the audio scheduler. They should be
reviewed separately, not enabled as part of the diagnostic contribution.

## Maintainer-requested test sequence

1. Review source changes and entitlement policy. No signing private keys,
   certificates or team credentials need to be shared with the tester.
2. Build and sign the **app** with the maintainer's authorized identity,
   including `com.apple.developer.driverkit.userclient-access` for
   `net.asfw.driver`. Verify the **embedded** entitlements on the final
   artifact, not just the source plist.
3. With the existing working driver unchanged, verify app-to-UserClient
   connection and graceful reporting of an unavailable/unsupported
   diagnostic. This step alone cannot validate selector 84.
4. Only after a separate driver code review and maintainers' controlled,
   signed driver deployment, verify selector 84 with an attached FF800.
   Confirm correct GUID, generation, identity, unavailable reasons and
   no polling. Capture logs and preserve the prior working setup according
   to the maintainer's established release/test procedure.
5. Keep FF800 hardware writes, phantom power switching and routing
   modifications disabled throughout these tests.

## Reproducible local commands (no driver activation)

```bash
cd ~/ASFireWire-Prototyp
git switch feature/fireface-800-controls
git pull
xcodebuild test -project ASFW.xcodeproj -scheme ASFW \
  -configuration Debug -destination 'platform=macOS,arch=arm64' \
  -only-testing:ASFWTests/FF800SessionDiagnosticWireTests \
  CODE_SIGNING_ALLOWED=NO
./build.sh --config Debug --no-bump
bash tools/ff800-connection-preflight.sh
```

These commands exercise the host tests/build and **read-only** preflight,
not the real selector. The user cannot sign with the maintainer's Team ID:
the Mac reports zero local signing identities.

## Review questions for the maintainer

- Would a minimal, signed **app-only** build for the existing driver be
  feasible, strictly to confirm the connection and the unsupported-selector
  behavior?
- Which base commit and ABI conventions should the 64-byte selector use?
- Would an isolated driver-side read-only selector be acceptable after
  scheduler/runtime review, and is selector 84 free in the maintainers'
  current tree?
- What is the preferred CI/signed tester distribution workflow?
- For eventual FF800 Settings/Router features, what hardware protocol
  evidence and validation rules would the maintainer require before any
  write path can be considered?
