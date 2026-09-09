# Protocol status

## Qualified STNC interface — 2026-09-09

The actual Windows server built by build.cmd uses stn_rpc_client.c and
stn_rpc_win32.c directly. It maintains a configured 127.0.0.1:18473 Chain
connection and polls MINING_TEMPLATE approximately once per second. INFO is now
available through the same client API; the server's work polling does not depend
on INFO. The standalone adapter/session prototype below is not the live server
composition and was not expanded in this qualification.

STNC v1 uses a 24-byte big-endian header: magic[4], version u16=1, kind u16,
opcode u16, result u16, request ID u64, payload length u32. Requests have kind 1
and result 0; responses have kind 2 and must echo the opcode/request ID. The
client validates framing, correlation and successful method lengths. Maximum
payload is 1,051,948 bytes, matching current Chain rather than the old 1 MiB bound.

| Operation | Request | Successful response |
| --- | --- | --- |
| 0x0001 INFO | empty | 176 bytes: network[32], genesis[32], height u64, tip[32], work[32], target[32], status u32, block count u32 |
| 0x2002 MINING_TEMPLATE | empty | base[32], work ID[32], block length u32, canonical v3 block |
| 0x2003 SUBMIT_WORK | complete template payload with only block nonce changed | block ID[32], accepted height u64, cumulative work[32] (72 bytes) |

Template fields inside the block include height at 72, target at 120, and the
big-endian 64-bit nonce at 152..159. The complete block starts at payload offset
68. Work ID and base are opaque authoritative Chain identities; Stratum copies
them and preserves all content outside the nonce. Chain alone decides validity.

Wire results 0..10 map explicitly to OK, INVALID, VERSION_ERROR, METHOD,
FORBIDDEN, UNAVAILABLE, NOT_FOUND, REJECTED, PROVIDER, CAPACITY, STALE. Error
payloads must be empty. Unknown codes and malformed successful responses reject.
Local transport failure is distinct (client code 11). Receive/send operations
have 5-second socket timeouts; connection establishment retains the operating
system's connect behavior. Only INFO/template reads get one reconnect/retry.
A lost solved-work response is an uncertain transport outcome and is never
silently retransmitted or reported accepted. Subsequent calls can reconnect.

Qualified with the actual Stratum server, an additional concurrent STNC client,
and a test driver linking the exact production Stratum client/transport sources:
INFO, template identity/fields, fixed-vector rejected/accepted/stale submissions,
unsupported method, no-work, outage/reconnect, and lost-mutation-response behavior.
No external miner, share handling, or production identity qualification is claimed.

## Qualified deterministic job mapping — Phase 10 Chunk 2

The current server's existing STNM v1 job representation is qualified against
actual Chain 0x2002 responses. It sends an 84-byte header followed by the complete
canonical block without reconstruction or alteration:

| Offset | Bytes | Source/meaning |
| --- | --- | --- |
| 0 | 4 | STNM |
| 4 | 1 | Version 1 |
| 5 | 1 | JOB type 1 |
| 6 | 2 | Zero reserved bytes |
| 8 | 32 | Exact Chain work ID; no second job-ID scheme |
| 40 | 32 | Exact Chain target from block offset 120 |
| 72 | 4 | Big-endian canonical block length |
| 76 | 8 | Initial nonce copied from block offset 152 |
| 84 | length | Entire unchanged canonical v3 block |

The protocol's fixed work rule permits a future miner to change only block bytes
152..159 as a big-endian uint64 (full 0..2^64-1 range). Job creation changes none
of those bytes; fresh nonce zero is preserved. No extranonce, coinbase rewrite,
header regeneration, compact target, or floating-point difficulty exists here.
Network, version, height and previous-tip identity remain in the unchanged block;
the Chain base also remains in Stratum's cached STNC prefix for later submission.

Identical Chain work yields identical job ID and complete transmitted bytes,
including across independent sessions. Changed Chain work IDs replace current
jobs; the server updates each session's sent ID. On UNAVAILABLE, the server clears
its current work and session job flags and does not send a cached/placeholder job
to old or new sessions. STNM v1 has no separate cancellation broadcast; receipt
of old bytes never grants authority to claim acceptance. Final validation remains
Chain's responsibility.

93 observation-only integration checks pass using the actual server and Chain
test runtime: initial no-work, one/two pending items, repeatability across three
sessions, exact ID/target/candidate mapping, replacement and subsequent no-work.
No miner computations or share messages were sent. Scripted identity hooks only
supply accepted test content; no production identity qualification is implied.
No mapping/parser defect required a production code change.

## Qualified miner-result return path — Phase 10 Chunk 3

The existing STNM result request is exactly 48 bytes: magic STNM at 0, version
u8=1 at 4, SUBMIT u8=2 at 5, zero reserved bytes at 6..7, Chain work ID[32] at 8,
and a big-endian uint64 nonce at 40. No candidate/target/height fields are accepted
from the submitting session. Stratum copies its cached authoritative STNC payload
and changes only canonical block bytes 152..159, then calls STNC 0x2003. A zero
nonce is permitted; the protocol neither truncates to 32 bits nor reserves prefixes.

Responses are 12 bytes: STNM, version 1, RESULT type 3, two zero reserved bytes,
then a big-endian u32 result at offset 8. Codes are 0 accepted, 1 Chain-rejected,
2 stale/unknown/no-current-job, 3 provider/transport/unavailable, and 4 malformed
protocol. Malformed magic/version/type or nonzero reserved fields now return 4;
they previously fell through to provider failure or ignored reserved bytes.

Only Chain's successful 0x2003 response produces accepted. Forwarding alone is
not acceptance. A mismatched work ID or absent current job yields stale before
forwarding. Chain can independently reject insufficient work or report stale
state after forwarding. Unavailable received from Chain maps to provider; a
server already holding no current job answers stale. Existing uncertain transport
outcomes are not retried as mutations or reinterpreted as success.

The 115-check return-path proof sends actual STNM results to the actual Stratum
server and obtains actual Chain rejection/acceptance through STNC. Accepted
persisted bytes equal the original Chain candidate plus only the nonce, including
an accepted nonce above 2^32 and the unchanged full target. Unknown, replaced,
malformed and reserved-bit requests are rejected; two concurrent sessions and a
reconnected session cannot substitute old work for the current job. Accepted
two-item pending cleanup and Chain restart are also checked.

The fixture generates bounded fixed-target results and uses the authorized
scripted-identity Chain runtime. This does not qualify a hardware/external miner,
production identity, share accounting or payouts. Chunk 4 has not started.

## Qualified failure/reconnect state — Phase 10 Chunk 4

Polling failures already clear current work and per-session job flags. A solved-
work response indicating STALE, TRANSPORT, UNAVAILABLE, PROVIDER, or INVALID now
also invalidates current work immediately, rather than waiting for a later poll.
Successful acceptance still clears it; a rejected nonce does not invalidate an
otherwise current template. No failed/uncertain communication is treated as
acceptance, and mutations are not queued or automatically retried.

Recovery fetches a fresh authoritative Chain template. Identical content/tip
restores identical job bytes and ID; changed work produces its Chain-defined
replacement. If Chain has no eligible work after restart, old and new sessions
receive no placeholder or cached current job. Previously received bytes are not
a cancellation/acceptance guarantee; STNM still has no cancellation broadcast.

A partial miner frame belongs only to its live socket. Disconnect discards it;
a new connection does not inherit it or its previous current-job state. Other
sessions and the shared authoritative candidate remain intact. Restarting the
actual Stratum process closes old paths and fetches Chain work anew.

148 actual-process checks qualify Chain outage (including a submitted result
returning provider, not accepted), empty reconnect, identical-work recovery,
changed work during miner disconnect, partial-frame disposal, bounded session
isolation, stale results and Stratum restart. Detection uses existing polling and
socket timeouts, not instantaneous outage detection. No discovery, alternate
endpoint, durable queue, large-scale stress or hardware-miner qualification.

## Earlier session prototype (historical details)

There is no public wire protocol in this increment. The implemented protocol
state is deliberately smaller than Bitcoin Stratum:

1. Initialize a session with an explicit uint64 identifier.
2. Subscribe to protocol version 1.
3. Install STN-Chain's exact `MINING_TEMPLATE` response payload.
4. Distribute the immutable template ID, target, height, and block bytes through
   a future transport without reinterpretation.
5. Submit a uint64 nonce against that job and the currently observed tip.

An inactive job yields `NO_JOB`; a changed tip yields `STALE`; a repeated nonce
yields `DUPLICATE`; bounded history exhaustion yields `CAPACITY`. The chain
adapter alone submits the exact `SUBMIT_WORK` payload and determines whether a candidate is accepted, rejected, stale,
or failed by its provider. A successful result exposes the exact candidate bytes.

Share target equals the chain target. Introducing pool difficulty before its
integer semantics and accounting are defined would create a false contract.

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
