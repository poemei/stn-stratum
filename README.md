# STN STRATUM
The Stratum interface for the **STN CHAIN**

 - Allows connections from CPU/GPU/ASIC
 - The STN CHAIN has a policy that states, ALL who participate, get compensated.
 - Designed to be platform/OS agnostic

## Current development state

Phase 10 Chunks 1–3 are qualified on Windows Release/x64. The live server uses
STNC INFO (`0x0001`), MINING_TEMPLATE (`0x2002`), and SUBMIT_WORK (`0x2003`).
Chain supplies authoritative work; Stratum transports jobs and returns results.
Only Chain determines acceptance. Work ID, candidate bytes, full target, metadata,
and the big-endian 64-bit nonce contract are preserved.

The hardware and compensation statements above describe project direction.
Hardware miners, share accounting, and payouts remain unqualified.

From an x64 Visual Studio developer command prompt in this repository:

```bat
build.cmd
build\test-chain-interface.exe --selftest
build\test-stratum.exe
```

Run `build\stn-stratum.exe` to start the server. It connects to Chain at
`127.0.0.1:18473`, listens on port `18475`, and writes
`logs\stn-stratum.log` relative to its working directory.

The cross-process checks reside in the adjacent Chain repository at
`tools/test-stratum-interface.ps1`. Run its default mode, `-JobMappingOnly`, and
`-MinerResultOnly` separately against the qualified Chain test executables.
These are fixture-based checks, not hardware-miner qualification.

See [Protocol](docs/PROTOCOL.md) for wire contracts and qualification limits and
[Architecture](docs/ARCHITECTURE.md) for the live executable boundary.
Chain-side development follows the approved **20260909.0 Chain Development
Policy**, supplied at `C:/stn-chain/policies/20260909.0_CHAIN.md`:
**Small. Deterministic. Easy to Use.**
