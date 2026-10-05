# World-state transport

The experimental host broadcasts a compressed full-world snapshot 20 times per second over RakNet using `UNRELIABLE_SEQUENCED`. RakNet fragments large messages to fit its negotiated datagram size; a lost fragment can discard that snapshot, after which a later full snapshot can recover the replica.

`WorldStateTransport::BroadcastSnapshot` can report the compressed application-payload size. The host accumulates average and peak payload sizes and estimates per-client payload bandwidth at 20 Hz in `WorldStateServer.log`. These values exclude RakNet, UDP/IP and link-layer headers, retransmissions, and terrain-patch traffic. Multiply the per-client estimate by the number of receivers for a rough server egress estimate.

## Debug measurement

Run the Windows debug executable from the game directory:

```text
Cortex Command.debug.minimal.exe -debug-run 60 -debug-run-output WorldStatePayloadMetrics
```

The run automatically loads Tutorial Bunker, creates a stress scene, writes screenshots and logs, and finishes the world-state checks. `DebugRun.log` includes the compressed payload and estimated payload bandwidth from a localhost loopback that sends the captured scene snapshot to four clients.

## Baseline

On 2026-10-05, a Windows x64 `Debug Minimal` run captured Tutorial Bunker with 361 top-level objects. The encoded snapshot was 29,633 bytes; LZ4 reduced the transmitted payload to 9,438 bytes. At 20 snapshots per second, this is an estimated **1,510 kbit/s per client** before transport overhead. The four-client localhost snapshot and terrain-patch loopback passed.

This is a local debug-build payload baseline, not a WAN throughput or packet-loss result. The host-session smoke test starts before the gameplay world is active, so its small empty-world snapshot is only evidence for connect, assignment, input, and disconnect handling; it is not representative of gameplay traffic. The measured full snapshots make delta/state-change encoding a priority before increasing scene size or player count.
