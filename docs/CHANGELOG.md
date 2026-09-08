# Changelog

## Unreleased

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
