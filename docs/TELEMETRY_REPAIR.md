# Telemetry connector repair - 2026-10-01

## Findings

The earlier mining repair did not adequately verify website telemetry. The
listener on port 18476 remained present, but it shared the server loop with
synchronous Chain RPC. Waiting for a Chain reply prevented HTTP requests from
being served. A public probe reproduced seven three-second timeouts in twenty
requests. Successful requests between stalls established that the listener was
intermittently blocked, rather than missing or blocked by a firewall.

The website had a second fault: its Home JavaScript requested `/home/status`, but
the Home module manifest registered only `index`. The router requires an explicit
route entry, so the existing public `status` method returned HTTP 404. The page
preserved its initial state after failed polling, leaving missing/stale telemetry
on screen. This manifest omission was not introduced by the preceding Stratum
deployment; that deployment did not edit the website.

## Narrow repair

- `includes/stn_rpc_linux.h`, `includes/stn_rpc_win32.h`: optional idle callback
  and context, initialized to null. Callback contract prohibits RPC reentry.
- `platforms/linux/stn_rpc_linux.c`, `platforms/windows/stn_rpc_win32.c`: wait for
  readable RPC bytes in 100 ms slices, service the callback, and enforce a
  monotonic deadline. Existing platform receive allowances remain bounded
  (Linux 60 seconds, Windows 5 seconds). Fragmented headers/payloads still read
  completely; RPC responses and mutation retry rules are unchanged.
- `src/stn_stratum_server.c`: connect the callback exclusively to the existing
  telemetry service. It observes current server fields in the same thread; it
  does not process mining submissions or make recursive Chain calls.
- `tests/test_telemetry_rpc_wait.c`, `Makefile`, `.gitignore`: real-socket regression
  with delayed, fragmented Chain header and payload, ten concurrent HTTP status
  requests, bounded HTTP timeouts and verification of the eventual RPC reply.
- `docs/CHANGELOG.md` and this report: evidence and deployment record.
- Website `C:/poes_projects/stn-chain.org/user/modules/home/module.json` and
  `/var/www/stn-chain.org/user/modules/home/module.json`: add only `status` to
  the existing public route allowlist. Existing controller/schema preserved.

## Validation and deployment

- Linux production build with `-Werror`: passed.
- New stalled-RPC telemetry test: passed, all ten HTTP replies received while
  the fake Chain delays its response. RPC payload preserved.
- Existing real-socket miner fairness regression: passed.
- Windows production build: passed. No Core executable changes in this repair.
- Installed Linux Stratum SHA256:
  `b1a1b058c3ec1ae31e9c16550dd00e946722e926e88f0e922e96738867d90d67`.
- Service `stn-stratum` restarted and verified active on chain01.
- Previous binary: `/usr/local/bin/stn-stratum.20261001T142459Z-telemetry`.
- Build/test source: `/home/stnchain/stn-stratum-repair-20261001`.
- Website manifest backup: `/home/stnchain/home-module-before-status-20261001.json`.

After deployment, 60 consecutive public `/status` requests succeeded, with zero
timeouts and measured response times from 0.250 to 0.687 seconds. Observations
included available/unavailable work and nonzero mining rates. The website's
`https://stn-chain.org/home/status` returned HTTP 200 JSON both from chain01 and
from the external Windows client. One verification response reported
`mining_available=true`, `mining_running=true`, one miner and 8758 H/s.

These are bounded observations. Telemetry reports the latest observed mining
state while an RPC is outstanding; it does not pretend the outstanding request
has completed. This fix addresses RPC receive waits, not DNS/connect/send stalls.
Chain provider-busy responses and mining backend latency are not solved by making
telemetry responsive. Zero hashrate while work is unavailable can be a real
reported value; it is distinct from an unreachable telemetry connector.

Source changes are local and deployed, not committed or pushed.
