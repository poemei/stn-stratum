# Protocol status

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
