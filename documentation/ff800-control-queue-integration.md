# FF800 control-write queue integration contract (prototype)

This branch does **not** enable Fireface 800 hardware control writes.
The UI and control service remain read-only; the async harness is test-only.

## Existing transport boundary

`Protocols::Ports::ProtocolRegisterIO::WriteBlockLEQuadlets` uses a route token
captured at dispatch and reports a stale generation if the route is replaced.
The config register is a **write-only**, three-quadlet FF800 register; a
successful transport callback cannot confirm that any setting (especially
phantom power) is physically active.

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
