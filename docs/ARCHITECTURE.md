# Architecture

## Current executable qualification

Phase 10 Chunk 3 qualifies the live return path without redesign: a miner-facing
session supplies the exact current Chain work ID and one 64-bit nonce. The server
copies authoritative cached work and replaces only the nonce bytes before STNC
submission. It cannot construct consensus content from session input, and only
Chain's accepted response authorizes reporting acceptance. Malformed result
handling now enforces zero reserved bytes and uses the existing protocol-error
result. Unknown/replaced jobs remain stale. See PROTOCOL.md for qualified tests.

Phase 10 Chunk 2 additionally qualifies the existing live server's deterministic
job serialization: Chain work ID, full target, initial nonce, length and exact
canonical block become one STNM job. Identity and content are invariant across
sessions. Changed work replaces the job; unavailable work clears server/session
current-job state without fabrication. The mapping introduces no consensus logic
or miner execution. See PROTOCOL.md for exact offsets and the cancellation limit.

Phase 10 Chunk 1 is complete for Windows Release/x64 STNC compatibility with
current STN-Chain. build.cmd compiles main, stn_stratum_server, stn_rpc_client,
and stn_rpc_win32; it does not compile stn_chain_rpc_adapter or link Chain
consensus sources. The live server requests authoritative templates and returns
candidate work through STNC. It does not read Chain persistence or decide block
validity. See PROTOCOL.md for the qualified boundary and reconnect rules.

The older session/adapter design below is separate from this executable's current
composition; its broader job/miner claims are not part of Chunk 1 qualification.

## Historical session/adapter prototype boundary

STN-Chain remains authoritative for block structure, block identity, PoW,
candidate validation, and admission. STN-Stratum links the portable STN-Chain
block codec and consumes the exact STN-Chain mining payload: base tip (32),
template ID (32), block length (4, big-endian), and canonical block bytes.

Job creation accepts only a structurally valid version-3 block whose work nonce
is zero and whose previous hash equals the supplied base tip. The complete block
and STN-Chain-supplied deterministic ID are copied into bounded owned storage.
Target and height are copied from decoded canonical fields.

A session ID is supplied by its owner; no clock, random source, pointer, thread,
or process state generates it. Duplicate nonces use a fixed insertion-ordered
array of 64 values. Capacity exhaustion is explicit. Candidate construction
copies the immutable template and changes only bytes 152 through 159 using
big-endian uint64 encoding. Submission reconstructs the exact 68-byte prefix and
passes the complete payload to the chain adapter.

## Historical prototype deferrals

In this separate prototype, socket transport, authentication, concurrency, miner
naming, wire framing, and job broadcast were deferred. The current live server's
transport, concurrent sessions, wire framing, and job mapping are covered above.
Economic rules do not exist: no reward, coinbase,
payout, fee, or reduced share target is claimed.

## Phase 10 — Chain ↔ Stratum Integration COMPLETE

Qualified on Windows Release/x64 (2026-09-09). Chain-issued work passes through
actual STNC 0x2002, deterministic STNM jobs, fixture miner results and Stratum's
STNC 0x2003 submission into independent Chain validation and persistence. Work
identity, full target and canonical candidate remain exact except the permitted
64-bit nonce. Invalid/stale results remain rejected; bounded sessions agree.

The final 125-check lifecycle stops Chain and Stratum once, recovers identical
accepted INFO and block bytes with Stratum absent, then starts a fresh coordinator.
Unavailable work yields no placeholder/cached current job. New height-two work
extends the recovered accepted tip with a new Chain work ID. Stratum coordinates
mining; STN Chain remains consensus authority. No production fix was necessary.

All 562 Phase 10 process checks (81/93/115/148/125), 1,544 Phase 9 checks,
1,130,094 Chain C checks, 27 parser checks, two session assertions and 34 build
probes pass. Release/x64 builds have zero warnings/errors. Identity and result
fixtures remain test-only; hardware, production identity, accounting, performance
and other platforms are not qualified. Phase 11 has not started.
