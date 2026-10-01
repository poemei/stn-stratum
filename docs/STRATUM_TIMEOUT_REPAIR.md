# Stratum disconnect repair - 2026-10-01

## Scope and findings

The reported symptom was recurring mining submission transport failures in STNC
Core. Inspection identified three interacting problems:

1. The Stratum server drained a client's input indefinitely before moving to the
   next client or refreshing work. A busy miner could starve those operations.
2. Core assumed the next twelve bytes following SUBMIT were a RESULT. STNM JOB
   notifications are asynchronous and can cross a submission. Reading the start
   of a valid JOB as RESULT falsely reported a transport failure.
3. Core's default 30-second socket timeout expired while Stratum still allowed
   its synchronous Chain RPC 60 seconds. A live failure at 30 seconds remained
   after the framing fix, confirming that both problems needed correction.

Chain also spends substantial time rebuilding/validating accepted history and
serving peer snapshots. Provider-busy responses and slow block acceptance remain
visible. These are not proof of a broken TCP connection and must not be reported
as successful submissions.

## Changes

Stratum files:

- `src/stn_stratum_server.c`: process one complete frame per client per server
  pass in both platform branches; preserve partial input. Remove a Unix-only
  header incorrectly included in the Windows branch.
- `tests/test_service_fairness.c`: real Linux socket/session regression for a
  busy client, a second client's reply, and fragmented input.
- `Makefile`: fairness test target and header dependencies for production objects.
- `build.cmd`: include existing session/listener/telemetry implementation files
  omitted from the Windows link.
- `.gitignore`: allow the new regression source to be tracked.
- `docs/CHANGELOG.md`, `docs/PROTOCOL.md`, this report: behavior and deployment.

Companion Core files in `C:/poes_projects/stnc-core`:

- `includes/stnc_stratum_client.h`: queued replacement job and a 75-second
  mining-session timeout.
- `src/stnc_stratum_client.c`: consume and validate interleaved JOB frames until
  the actual RESULT arrives; retain the newest replacement; free it on disconnect.
  Apply the timeout only to this mining connection.
- `tests/test_stnc_stratum_client.c`: JOB/JOB/RESULT ordering, queued-job delivery,
  malformed input rejection and timeout selection.
- `docs/CHANGELOG.md`: record companion repair.

The generic Windows socket implementation was not changed. No wire format,
signature rule, contract, Chain consensus rule, wallet or identity was changed
by this repair. Existing uncommitted contract work was preserved.

## Validation

- Linux production build with warnings treated as errors: passed.
- Linux `test-service-fairness`: passed.
- Linux `test-client-session`: 22 checks, zero failures.
- Linux `test-job-refresh`: passed.
- Linux `test-miner-job`: 16 checks, zero failures.
- Windows Stratum production build: passed.
- Core full Windows build/test suite after framing fix: passed, including the
  existing contract view/window tests.
- Core final production rebuild and focused Stratum client test after the
  75-second timeout adjustment: passed.
- Whitespace/error checks on both repository diffs: passed.

## Deployment

Stratum was installed and restarted on chain01 at 2026-10-01 07:32:39 UTC.

- Service: `stn-stratum`, verified active.
- Installed binary: `/usr/local/bin/stn-stratum`.
- SHA256: `76199e1250bbe32b740dd3b6e2af76f8208f878946edf4d66bacfc25d2d40f71`.
- Previous binary: `/usr/local/bin/stn-stratum.20261001T073239Z-fairness`.
- Source/build evidence: `/home/stnchain/stn-stratum-repair-20261001`.
- Existing service configuration and ports were preserved.

The final Core executable was copied into the POE, stnc-treasurer and STN-Labz
desktop profiles, which were closed normally and restarted around 07:37:45 UTC.

- Core SHA256: `de22de3a667698140c71e79ac4a964edfb7fd607eed21ae868990a0bb61b7c03`.
- Each profile retains `stnc-core.exe.pre-stratum-20261001.bak`.
- Wallets, identities, configurations and saved mining preferences were preserved.
- Only stnc-treasurer has mining enabled in its saved configuration. POE and
  STN-Labz were not enabled for the purpose of this test.

## Live verification and limits

From the final Core restart around 07:37:45 UTC through 07:45:16 UTC:

- Active stnc-treasurer miner: 12 accepted shares, one mining connection, zero
  mining transport failures/reconnects. Eight stale-work replies were handled
  on the existing session instead of reconnecting.
- All three Core profiles: zero logged Chain reconnect failures in this window.
- POE and STN-Labz had mining disabled, so no mining results are claimed for them.
- The final automated observation ran 181 seconds and passed; counters include
  the entire final application run, not just that final three-minute window.
- Stratum server logs independently show Chain-verified shares and accepted block
  submissions with result 0 and 80-byte acceptance responses during this run.
- All three installed desktop executable hashes match the final Core build.

These are bounded live observations, not a long-duration uptime guarantee.

Stratum's Chain RPC remains synchronous. Fair scheduling prevents unbounded
draining of one miner's input, but does not remove backend head-of-line blocking
or guarantee response latency under arbitrary load. Other, unmodified solo
clients continued to reconnect during observation; their client implementation
was not updated by this repair. Do not interpret the Core verification as proof
that every external miner has been fixed.

Chain still intermittently returns provider-busy code 8 and Core still reports
unsuccessful P2P peer qualification. A live accepted share does not establish full
P2P synchronization or guarantee that Core's overall network status is healthy.
Those remaining Chain synchronization issues are separate from the verified
Stratum client framing/deadline repair.

Source changes remain uncommitted and have not been pushed to GitHub.
