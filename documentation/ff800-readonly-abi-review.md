# FF800 read-only diagnostic ABI: isolated review slice

This branch starts from `main`, **not** from the broad experimental
`feature/fireface-800-controls` branch. It contains only a proposed
process-local diagnostic packet declaration and an independent host-only
size/layout smoke test. No DriverKit implementation, UserClient selector,
Swift UI, FF800 register access, settings write or routing change is added.

## Purpose

Before discussing selector 84 or a signed hardware build, agree on an
explicit, versioned, 64-byte FF800 *observation* packet. The proposed
values distinguish an unavailable response from a best-effort diagnostic.
An `EligiblePreviewOnly` reason is never permission to write to hardware.
Neither a timestamp nor the `routeCurrent` flag establishes stable route
ownership or real hardware readback.

Source:
- `ASFWDriver/Shared/ASFWFF800SessionDiagnosticsABI.h`
- `tests/devices/FF800SessionDiagnosticABISmoke.cpp`

## Standalone smoke test (macOS or Linux with a C++17 compiler)

```sh
c++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  tests/devices/FF800SessionDiagnosticABISmoke.cpp \
  -o /tmp/asfw-ff800-abi-smoke
/tmp/asfw-ff800-abi-smoke
```

This compile/run step needs no Xcode signing, hardware, DriverKit, driver
installation, or access to the running UserClient. In particular, **it
does not prove the packet is implemented by the installed driver**.
Existing app/driver tests should remain unaffected because this contract
is not wired into production targets.

## Maintainer review questions

1. Is a dedicated diagnostic selector useful, given the recent RME control
   development already underway in upstream issue #230?
2. Is the proposed `ASFWFF800SessionDiagV1` shape appropriate, or should
   this reuse the project's existing diagnostic/telemetry ABI instead?
3. Is a little-endian, Apple-local, 64-byte wire structure suitable across
   target architectures, or should an explicitly serialized byte encoding
   be required?
4. Only **if** the contract is accepted: choose the selector number from
   the then-current upstream registry and review the runtime implementation
   as an independent follow-up. The experimental branch's selector 84 is
   **not claimed to be free in current upstream**.

Related discussion: https://github.com/mrmidi/ASFireWire/issues/230

This branch is offered for review, **not as an activation or installation
procedure**. The tester's existing signed 0.6.0/33 driver stays untouched.
