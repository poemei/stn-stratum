# Changelog

## Unreleased

### Phase 10 Chunk 5 — full integration COMPLETE — 2026-09-09

- Added 125 actual-process checks (-FullLifecycleOnly) covering Chain template,
  deterministic Stratum jobs, fixture results, STNC solved-work, independent
  Chain acceptance, exact persistence and new work from recovered accepted state.
  Work ID, full target and candidate remain exact except the 64-bit nonce.
- One bounded interruption stops both processes. Chain recovers identical INFO
  and accepted block bytes before Stratum restarts. No unavailable placeholder
  or accepted old job becomes current; invalid/stale results remain rejected.
- No production defect found or production code changed in this chunk. Phase 10
  COMPLETE is documented in roadmap, architecture and protocol documentation.
- Windows Release/x64 rebuilds: zero warnings/errors. All 125 new and 437 prior
  Phase 10 checks, 1,544 Phase 9 checks, 1,130,094 Chain C checks, 27 parser checks,
  two session assertions and 34 build probes pass with zero failures.
- Scripted identity/result fixtures remain test-only. Hardware, production
  identity, accounting, performance and other platforms remain unqualified.
  No commit or push performed. Phase 11 not started.


### Failure/reconnect state qualification — 2026-09-09

- Fixed a stale-current-state window: failed/uncertain solved-work calls left
  have_work/session flags current until polling. STALE/TRANSPORT/UNAVAILABLE/
  PROVIDER/INVALID results now invalidate those flags immediately. Ordinary
  rejected nonces preserve valid current work. No new retry or consensus rules.
- 148 new actual-process checks pass: Chain loss and attempted submission,
  no false acceptance/current work, empty reconnect, identical-content recovery,
  changed work during miner disconnect, stale results, partial message disposal,
  bounded session isolation and actual Stratum process restart. Test shutdown
  accepts both EOF and Windows connection reset as disconnection, not timeout.
- Prior Chunk 1/2/3 suites (81/93/115), 27 parser checks and two session assertions
  remain passing; Phase 10 cross-process total is 437 checks. Optimized Windows
  x64 builds have zero warnings/errors, zero failures. No additional platform
  qualification or hardware-miner claims.
- Updated protocol failure semantics. Existing independent README/source changes
  preserved. No Chunk 5, discovery, durable queues, accounting, commit or push.

### Stratum baseline refresh — 2026-09-09

- Rebuilt the current Windows x64 executable and test drivers with C17 /W4 /WX;
  no warnings or errors. Re-ran 27 parser checks, two session assertions, and
  all three actual Chain/Stratum integration modes: 81 interface, 93 job mapping,
  and 115 result-path checks (289 integration checks), zero failures.
- Added current qualification limits, build/run instructions, and the supplied
  20260909.0 Chain Development Policy reference to README. Clarified historical
  prototype deferrals so they do not contradict the qualified live server.
- No production source or wire-contract changes were needed for this refresh.
  Existing uncommitted Chunks 1–3 changes are preserved. Chain source and miner
  were not modified; hardware miners and accounting remain unqualified.
  This refresh does not claim Chunk 4 implementation. No commit or push.

### Miner result to Chain return path — 2026-09-09

- Qualified the existing actual STNM result -> cached candidate plus nonce ->
  STNC 0x2003 -> Chain validation path. No consensus logic or new result identity.
- Fixed two narrowly scoped parser/result defects: nonzero reserved result bytes
  were ignored, and malformed/invalid protocol results mapped to provider instead
  of the already-defined protocol-error code. Added real-wire regressions for both.
- 115 integration checks pass: exact work ID, full-width big-endian nonce above
  2^32, unchanged candidate/target verified in Chain's persisted accepted block,
  Chain rejection of forwarded invalid PoW, unknown/stale/malformed requests,
  session isolation and reconnect, two-item pending cleanup and restart.
- Windows optimized x64 builds: zero warnings/errors. Prior 93 job-mapping and
  81 interface checks, 27 parser checks and two session assertions pass. Combined
  Phase 10 integration is 289 checks, zero failures. No hardware miner, production
  identity, share/reward/economic changes or Chunk 4 work.
- Updated protocol/architecture documentation. No commit or push performed.

### Deterministic miner-job mapping — 2026-09-09

- Qualified the actual server's existing 84-byte STNM job header and unchanged
  canonical block against real Chain templates. Exact work ID, 256-bit target,
  initial nonce, previous tip, height/version/network and candidate bytes survive.
  No competing identity, consensus interpretation or miner execution was added.
- 93 new observation-only checks in Chain's existing integration infrastructure
  cover initial unavailable, repeated templates, three-session consistency,
  pending-driven job replacement and subsequent unavailable without cached jobs.
  No mapping defect or production code change was required. Documented exact
  nonce contract and absence of a separate STNM cancellation broadcast.
- Windows optimized x64 builds pass with zero warnings/errors. Existing 27
  parser checks, two session assertions and 81 Chunk 1 integration checks pass;
  combined cross-process qualification is 174 checks, zero failures.
- Updated PROTOCOL.md and ARCHITECTURE.md. Chunk 2 complete; no Chunk 3, actual
  miner/shares, economics, protocol redesign, commit or push.

### Chain STNC compatibility qualification — 2026-09-09

- Reconciled the actual live client's payload maximum to 1,051,948 bytes; added
  the missing 176-byte INFO operation. Enforced response allocation/frame bounds,
  INFO/submit lengths, and template length/magic/version instead of accepting
  malformed successful payloads. No consensus checks moved into Stratum.
- Added five-second send/receive socket timeouts. Restricted automatic replay
  after disconnect to read-only INFO/template operations; a lost mutation response
  remains an explicit uncertain transport failure. Later calls can reconnect.
- Added a driver linking the production client/transport and 27 parser/status
  regressions. build.cmd now builds driver and existing session smoke tests, with
  intermediate object files kept under build/. Windows optimized x64 /W4 /WX
  builds pass with zero warnings/errors; 27 checks plus two existing session
  assertions pass. The production server was exercised alongside the driver.
- Chain's tools/test-stratum-interface.ps1 passes 81 actual integration checks:
  configured connection, INFO, complete work identity/target/nonce, deterministic
  accepted/rejected/stale results, unknown opcode, no-work, concurrent RPC,
  outage/reconnect, and no mutation retransmission after a lost reply.
- Updated PROTOCOL.md and ARCHITECTURE.md. No job/share/miner expansion, protocol
  redesign, production identity work, commit, or push. Chunk 1 only is qualified.

- Established the ISO C17 STN-Stratum repository and Visual Studio solution.
- Added bounded deterministic session, job, stale-work, duplicate-submission,
  canonical nonce mutation, and chain-adapter state handling.
- Reused STN-Chain's authoritative block codec rather than cloning consensus.
- Added 16 deterministic checks covering session versioning, repeatable jobs,
  immutable templates, accepted/rejected/duplicate/stale/no-job submissions,
  and malformed/profile/base rejection.
- Documented the implemented architecture and explicitly deferred wire transport,
  RPC integration, rewards, payouts, and variable share difficulty because the
  corresponding STN-Chain contracts are not implemented.
- Qualified the initial increment with a warning-as-error Visual Studio 2026
  Release/x64 build and 16 passing checks.
- Aligned the job and submission boundary with STN-Chain's implemented 68-byte
  mining RPC payload, preserving its template ID instead of recomputing one.
- Requalified the aligned boundary with a clean warning-as-error Release/x64
  rebuild and 16 passing checks.
