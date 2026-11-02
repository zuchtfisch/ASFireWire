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
