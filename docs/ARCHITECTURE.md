# Architecture

## Implemented boundary

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

## Deliberately deferred

Socket transport, authentication, concurrency, miner naming, wire framing, and job
broadcast are also deferred. Economic rules do not exist: no reward, coinbase,
payout, fee, or reduced share target is claimed.
