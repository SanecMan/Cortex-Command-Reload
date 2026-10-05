# World-state transport

The experimental host broadcasts a compressed full-world snapshot 20 times per second over RakNet using `UNRELIABLE_SEQUENCED`. RakNet fragments large messages to fit its negotiated datagram size; a lost fragment can discard that snapshot, after which a later full snapshot can recover the replica.

`WorldStateTransport::BroadcastSnapshot` reports the compressed application-payload size and separate elapsed times for snapshot encoding, LZ4 compression, and queuing the message in RakNet. The host accumulates average and peak values for each stage and payload bandwidth in `WorldStateServer.log`. The measurements exclude snapshot capture and later socket transmission. Payload bandwidth excludes RakNet, UDP/IP and link-layer headers, retransmissions, and terrain-patch traffic. Multiply the per-client estimate by the number of receivers for a rough server egress estimate.

## Debug measurement

Run the Windows debug executable from the game directory:

```text
Cortex Command.debug.minimal.exe -debug-run 60 -debug-run-output WorldStatePayloadMetrics
```

The run automatically loads Tutorial Bunker, creates a stress scene, writes screenshots and logs, and finishes the world-state checks. `DebugRun.log` includes the compressed payload, estimated payload bandwidth, and separate `snapshot_encode_us`, `snapshot_compress_us`, and `snapshot_queue_us` measurements from a localhost loopback that sends the captured scene snapshot to four clients.

## Baseline

On 2026-10-05, a Windows x64 `Debug Minimal` run captured Tutorial Bunker with 361 top-level objects. The encoded snapshot was 29,633 bytes; LZ4 reduced the transmitted payload to 9,438 bytes. At 20 snapshots per second, this is an estimated **1,510 kbit/s per client** before transport overhead. The four-client localhost snapshot and terrain-patch loopback passed.

This is a local debug-build payload baseline, not a WAN throughput or packet-loss result. The host-session smoke test starts before the gameplay world is active, so its small empty-world snapshot is only evidence for connect, assignment, input, and disconnect handling; it is not representative of gameplay traffic. The measured full snapshots make delta/state-change encoding a priority before increasing scene size or player count.

## Snapshot serialization experiment

`EncodeSnapshot` now writes the body directly into the final packet buffer instead of building a second temporary body vector and copying it behind the header. The protocol header, v6 wire format, validation rules, and duplicate network-ID checks are unchanged.

An initial Windows `Debug Minimal` run before this change measured one 369-object snapshot at 3,955 us encoding, 3,891 us compression, and 70 us queueing. Two successful runs after the change measured 373 objects at 2,994/3,388/62 us and 372 objects at 3,230/3,027/52 us, respectively. Their means were 3,112 us encoding, 3,208 us compression, and 57 us queueing. Both runs passed the protocol and four-client transport checks and the full 60-update gameplay smoke. These are debug-build single-snapshot samples on slightly different scenes, so the apparent ~19% reduction in combined encode/compress/queue time is preliminary, not a controlled before/after benchmark. Repeat in a controlled Release benchmark before treating it as a confirmed performance gain.
