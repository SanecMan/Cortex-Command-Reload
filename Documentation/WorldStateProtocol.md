# World-state protocol foundation

The new protocol starts with explicit little-endian serialization rather than sending C++ structs or framebuffer pixels. Version 1 defines a full `WorldSnapshot` packet with a server tick, scene revision, UTF-8 activity and scene preset names, and a list of objects. Each object record carries a stable server-assigned network ID, class/module/preset names, transform and velocity, health, team, and flags.

The 16-byte packet header contains the `CCR1` magic, protocol version, message type, reserved byte, sequence number, and payload length. The decoder rejects unknown versions and message types, truncated or oversized packets, invalid UTF-8, non-finite numeric values, duplicate/zero object IDs, and object/string counts beyond fixed bounds. It does not rely on compiler struct packing or endianness.

`-debug-run` performs an in-process round-trip check with a Cyrillic preset name and verifies that truncated packets, unsupported versions, and duplicate network IDs are rejected. The codec is transport-independent so it can be carried over the repository's RakNet dependency.

This is the wire-format foundation only. The existing multiplayer sources remain excluded from both build systems and implement a remote-frame protocol. No server/client transport, object lifecycle reconciliation, input authority, terrain delta replication, or headless simulation is enabled by this codec yet; those pieces must be integrated before this protocol is exposed to players.
