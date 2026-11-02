# FF800 control-write queue integration contract (prototype)

This branch does **not** enable Fireface 800 hardware control writes.
The UI and control service remain read-only; the async harness is test-only.

## Existing transport boundary

`Protocols::Ports::ProtocolRegisterIO::WriteBlockLEQuadlets` uses a route token
captured at dispatch and reports a stale generation if the route is replaced.
The config register is a **write-only**, three-quadlet FF800 register; a
successful transport callback cannot confirm that any setting (especially
phantom power) is physically active.

## Confirmed audio-session execution model (code review)

- `AudioSessions::Ensure(guid)` creates one `IODispatchQueue` per GUID
  (`com.asfw.audio.session`). `AudioSessions` owns the queue and drains it
  on teardown and erase.
- `SessionScheduler::ArmPendingTimer` receives a timer callback on the
  Default queue and posts **deferred restarts** to that per-device queue.
- **The queue does not serialize every audio operation.**
  `AudioSessions::Attach/Detach/ChangeClock/RequestRestart` forward directly
  to `SessionScheduler`. Its own documentation says the caller that finds no
  reconcile in progress runs it **on the caller's thread**, while subsequent
  requests coalesce under a lock. Therefore posting a control write to the
  restart queue alone would not exclude concurrent Configure/Stop/Start.
- `FirefaceFamilyDriver::RunFF800Stage` blocks its family/session stage
  while bus completions and timer work progress elsewhere. Do not block a
  queue needed to dispatch those completions.
- `FirefaceDeviceProtocol::UpdateRuntimeContext` calls
  `RefreshRouteGeneration`; `SyncGeneration` clears cached flash settings,
  last transmitted configuration and control-write state on generation change.

### Read-only production session coordination snapshot

`SessionScheduler::CoordinationSnapshot()` now copies audio-relevant state
from the **existing scheduler lock** into a diagnostic structure:
HAL attachment, running/reconciling state, restart and clock-change requests,
pending reconcile tickets, device retirement, and teardown. Its counterpart
`AudioSessions::CoordinationSnapshot(guid)` reads only an **existing**
session via `Find`; unlike `Ensure`, it does not create one.

This is a read-only production code path, deliberately **not** used for FF800
write admission. The retired/teardown flags are atomics sampled outside the
scheduler lock; and even fields copied under the lock may change immediately
after its release. A snapshot that appears idle therefore cannot grant a
configuration lease or authorize a control write. The C++ session scheduler
host tests exercise queries while a device prepare stage is held, after
reconciliation, on an unknown GUID, and on retirement/teardown. No hardware
register writes are introduced.

### Read-only bridge from scheduler diagnostics to FF800 preview

`FF800ControlSessionDiagnostics.hpp` now maps an **existing**
`SessionCoordinationSnapshot` and separately supplied device/route diagnostics
to the pure FF800 preview policy. The adapter rejects unknown sessions, active
HAL/stream/reconcile/restart/clock states, retirement, teardown, disconnected
or stale routes, and **outstanding requested versus completed scheduler tickets**.
The mapping is exercised against live host-test `AudioSessions` snapshots at
idle, during a held start stage, while streaming, after StopIO and at retirement.
Additional host tests exercise a held StopIO, cancellation/teardown, and route
invalidation on bus reset. The bus-reset test deliberately demonstrates that
reusing *both* old session and old route facts can still yield a positive
preview; only fresh, independently validated route facts can veto it. This
is why the preview can never be used as a transport write permit.

The caller must supply genuine route/cache facts: the adapter does not read
the bus, discover nodes, acquire session locks across dispatch, or validate
cached flash settings. The adapter is **not wired into UI controls, control
services, or FireWire register writes**. It offers a diagnostic *preview*, not
a lease or authorization. Snapshot and route facts are not atomic together;
even a true result is immediately stale and MUST NEVER be used alone to
dispatch control writes. No FF800 hardware-write enablement is implied.

### Prototype-only admission gate

`FF800ControlSessionGate.hpp` is a pure policy preview, tested without
hardware. It refuses when CoreAudio is attached, audio is streaming, a
reconcile/restart/clock change is pending, the device is retired or tearing
down, or the route generation differs from the cached generation. **It is
not connected to the production session state or transport and cannot grant
permission to write.** A plain snapshot of these flags is insufficient:
production integration must obtain an exclusive per-device configuration
lease coordinated with `SessionScheduler::Submit/Reconcile` and recheck
generation at dispatch. Even such a lease would not solve the missing full
readback verification of the write-only FF800 config register.

### Simulated exclusive-lease contract (host tests only)

`FF800SimulatedConfigLease.hpp` is a **single-threaded reference model** for
how a future per-device configuration lease should behave:
acquisition needs a quiescent, current session; an acquired token excludes a
simulated audio mutation; an in-progress simulated audio mutation excludes a
new token; releasing an older token cannot free the current owner; invalidation
on bus reset revokes the old token even if a generation is later reused.

### Audio-priority preemption (reference model only)

A lease that blocks a real CoreAudio StartIO/StopIO, a bus reset, or
device teardown would be unacceptable. The host-only model therefore also
provides `PreemptForAudioMutation` and `PreemptForDeviceLoss`. Both
invalidate the control token/epoch immediately without waiting for a bus
callback. The former records that the simulated audio mutation is busy; the
latter lets an externally updated retired/offline/generation state refuse a
new admission. Tests cover a late release after preemption and repeated
preemptions.

**This does not yet safely cancel an actual in-flight FireWire write.** A
write that was already submitted may reach hardware even after preemption,
and FF800 has no reliable full config readback. A real design needs atomic
admission and dispatch arbitration with audio's reconcile owner, epoch-aware
transport cancellation/teardown, and a policy for writes that cannot be
reliably recalled. These are reasons to keep production controls read-only,
not reasons to defer or block audio operations.

This class neither acquires `SessionScheduler::lock_` nor makes
`Submit/Reconcile` atomic with FF800 control-write admission. It is **not**
used by production code, and it is not itself thread-safe. The actual
audio-session scheduler is free to reconcile on a caller thread; adding the
simulated lease to a callback queue would therefore not prevent real races.
A real design needs a shared per-GUID coordination mechanism in the scheduler
covering both audio mutation admission and control leases, with teardown and
device retirement priority. Never hold a lock needed by FireWire completions
while waiting for a device response. Without reliable full-image verification,
real FF800 setting writes must stay disabled even if this concurrency blocker
is eventually solved.

## Required controller-queue contract before any real integration

1. Run preview/baseline checks, request preparation, dispatch ownership, timeout
   and cancellation handling, and state mutation on **one serial controller
   execution context**. Do not read/write `FF800ControlWriteState` from a raw
   asynchronous bus callback.
2. Capture request ID, bus generation, and a per-controller epoch when
   dispatching. The bus callback must **post back** to the same context before
   touching state. Reject stale or repeated completion callbacks.
3. Obtain a fresh saved-settings cache after bus reset, and require a matching
   recorded configuration image. This is a preview precondition, **not proof
   of applied hardware state**.
4. Never automatically retry a failed, timed-out, canceled, or unverifiable
   full-image write. The actual hardware state may be unknown.
5. Do not mark success from a transport ACK or from the existing partial FF800
   status mirror. Full readback is not established.
6. Close the callback owner's lifetime before deallocating any captured
   transport, scheduler, or registry objects. Late callbacks must not access
   destroyed state.
7. Decide and document interactions with ongoing audio Configure/Start/Stop
   and bus rebind **before** enabling writes. Do not interleave full config
   images with those operations.

## What tests presently cover

The host-only harness takes an injected transport and optional callback-posting
function. Tests simulate queued callbacks, duplicate and reordered completions,
bus resets with reused IDs, timeouts, cancellations, and owner destruction.
They do **not** exercise real driver controller-queue scheduling, FireWire bus
traffic, or hardware verification.

No production driver call invokes the harness. Do not wire it to
`WriteBlockLEQuadlets` until the actual controller-queue owner, route lifetime,
serialization with audio Configure, and safe verification strategy have been
established and reviewed.
