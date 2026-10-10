# Separate-stream routing prototype

This is the first implemented building block toward independent main-screen
CarPlay and cluster phone maps on MOI3. It is C11 source with a host test
harness. It is **not a working CarPlay receiver, drop-in ScreenStream library,
or firmware update**. No proprietary firmware payload is included.

## What is implemented

`cp_router.c` gives each of two roles its own sink callbacks, context, display
dimensions, codec setup, frames, timestamps and lifecycle. The adapter supplies
the negotiated display and stream-to-role association. The core never guesses
which received frame is a map.

- Main operation does not require a bound cluster stream.
- Each role must use a distinct sink context.
- Cluster stop, disconnect, configure failure or frame-submit failure closes
  only the cluster sink. Main frames can continue.
- A restarted stream requires codec configuration before accepting frames.
- Unbinding or resetting invalidates stream handles. Late events from a
  disconnected phone session cannot address a newly bound stream.
- Callbacks synchronously borrow data. A decoder that queues work must copy
  or retain data before returning. Callers serialize all calls; callbacks must
  not reenter the router. The router does not create threads or queues.

Sink isolation is necessary, but a callback that blocks still blocks the
owner thread. A vehicle adapter will need bounded submission and its own
resource scheduling. Pointer inequality cannot establish hardware decoder
independence; that must be verified by the adapter.

## Build and checks

```sh
make -C mib3-research/prototype all test
make -C mib3-research/prototype sanitize
```

`all` builds a static library for the compiler's target. The default compiler
here is the host compiler, **not an ARM64 vehicle toolchain**. Tests use fake
sinks and synthetic payloads; they do not decode H.264, connect an iPhone,
render maps or exercise AVB. Fixture dimensions are not vehicle measurements.

The host tests passed with warnings treated as errors and with Undefined
Behavior Sanitizer. Address Sanitizer was attempted but its leak-check runtime
could not access `/proc` in the execution environment; that run is not counted
as passing. `make asan` remains available on a suitable development host.

## How this must be connected to MOI3

1. Resolve the actual secondary-display advertisement and negotiation ABI in
   the supplied AirPlay/SAL versions. Establish a stream-to-display mapping;
   do not assign roles by arrival order. Preserve the existing main descriptor.
2. Implement a native adapter from each received stream context to a router
   handle. Attach the original main output and a separate cluster decoder sink.
   This prototype deliberately does not export proprietary symbol names with
   guessed signatures.
3. Supply a real cluster surface via RSI/IVI, verified dimensions and an existing
   validated AVB destination. `x-navfpk` internal ID 37 is not a transport ID.
   Confirm ownership and coexistence with factory navigation before use.
4. Confirm updater acceptance, ARM64 target dependencies and restoration on an
   equivalent development unit before producing a component update.

None of these four integration steps has been completed by this prototype.
In particular, routing two synthetic streams does not make the iPhone send a
second stream. Audio and navigation metadata are outside this core.

## Additional observations from the 334.0.4 ScreenStream binary

Examined binary SHA-256:
`c74df3b91d9c27437dfd1a651be8876b2e5b22e26a0ff023ab83c312b4be52a8`.
The ELF is AArch64 even though the archive's package manifest labels it amd64.
Use the actual ELF header when choosing the target toolchain.

`SetStreamScreenDelegate` at virtual address `0x13c4` copies 56 bytes to a
global table. Register flow observed in `ScreenStreamProcessData`:

| Table offset | Observed use |
| --- | --- |
| `+0x00` | Shared callback context |
| `+0x08` | Dimensions callback; width, height, zero, shared context |
| `+0x10` | Initialization callback; shared context |
| `+0x18` | Codec callback; stored AVCC bytes, length, shared context |
| `+0x20` | Frame callback; converted timestamp, bytes, length, shared context |
| `+0x28` | Start callback; shared context |
| `+0x30` | Stop callback; shared context |

The timestamp conversion PLT import is `UpTicksToNanoseconds`. The prototype
passes timestamps unchanged; the native adapter must establish the timebase.

The stream's allocated 40-byte context stores a delegate context at `+0x00`,
AVCC pointer/length at `+0x08/+0x10`, dimensions at `+0x18/+0x1c` and initialized
flag at `+0x20`. In the inspected frame callback path, the context argument
comes from the global table, not the stored per-stream delegate context.
Merely changing `ScreenStreamSetDelegateContext` therefore does not establish
independent outputs along that path.

These are disassembly observations, **not verified C declarations or patch
instructions**. They explain why the prototype owns callbacks per stream.
