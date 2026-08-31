# iOS performance lab

This document describes the private QA build used to measure iOS performance
changes before any of them are enabled in the public port. It is not a user
configuration surface.

Configure with `OPENGOTHIC_IOS_PERF_LAB=ON`. This also enables the existing
low-frequency performance diagnostics. The build reads hidden selectors from
the `[IOS_PERF_LAB]` section of the writable `Gothic.ini` overlay. Missing or
invalid values preserve the public-port behavior.

```ini
[IOS_PERF_LAB]
workerParticipants=0
skyLutInterval=1
fogLutProfile=0
worldFarPlane=100000
waterReflectionMode=0
npcPoseMode=0
```

Selectors:

- `workerParticipants`: `0` for automatic, or `1`, `2`, `3`, `4`, `6` total
  participants including the main thread.
- `skyLutInterval`: update every `1`, `2`, `4`, or `6` rendered frames.
- `fogLutProfile`: low-quality sizes are `0` = 160x90x64,
  `1` = 120x68x48 and `2` = 96x54x32. HQ and epipolar sizes are
  respectively 128x64x32, 96x48x24 and 64x32x16.
- `worldFarPlane`: `100000`, `80000`, or `60000` Gothic units.
- `waterReflectionMode`: `0` = current masked full-screen pass,
  `1` = disabled control. Mode `2` is rejected: the expensive SSR path is
  already gated to water pixels, and a geometry-only prototype would require
  a separate renderer project.
- `npcPoseMode`: `0` = defer full skeletal-pose work for distant, off-screen
  NPCs while advancing animation events, `1` = full pose for every NPC as the
  control variant.

## Measurement protocol

Use the same signed binary and change one selector at a time. Restart the app
after editing `Gothic.ini`. Run short A/B/B/A captures from a nominal thermal
state, then validate the winning variants for 30–45 minutes on battery without
continuous console streaming. Keep the save, camera route, brightness, FPS
limit and graphics profile unchanged.

Record frame p50/p95/p99, CPU animation and pose-refresh p95, fence misses,
worker wakeups, active GPU time, thermal state and battery change. A variant is
not accepted based on FPS alone. It must avoid gameplay and image regressions
and provide a repeatable timing, energy or thermal improvement.

Shadow settings are intentionally outside this experiment.

Use both fixed physical-device scenarios for every candidate that can affect
rendering or CPU load:

- save slot 1: the shadow-heavy scene;
- save slot 4: the water and reflection scene.

Compare A and B only within the same save slot. Never combine results from the
two scenes into one FPS average.

## Bounded physical-device runner

`tools/ios/perf-lab/run-device-sample.sh` runs one repeatable sample without
installing or uninstalling an app and without touching a save directory. It
requires an already installed, container-accessible performance-lab build and
an existing save in slot 1 or 4.

```sh
tools/ios/perf-lab/run-device-sample.sh \
  --device 00008130-000000000000001C \
  --bundle opengothic.gothic2.perflab \
  --slot 4 \
  --profile /absolute/path/to/worker-2.ini \
  --duration-seconds 120 \
  --output-dir /absolute/path/to/performance-lab-results
```

The runner refuses to continue when the target app is already running or when
it cannot first download the current `Documents/Gothic.ini`. It snapshots the
selected profile, backs up the device INI, applies and verifies the profile,
launches `-nomenu -save`, terminates only the PID it observed after that launch,
then restores and verifies the original INI. It never uses `uninstall`,
`--terminate-existing`, a process-name kill, or any path below `savegames`.

App stdout is redirected to `raw-console.log` by a collector bounded to the
requested duration; it is not displayed as an open-ended console stream. Each
run directory also retains the original and applied INI copies, stable JSON
results from `devicectl`, the filtered `world-telemetry.log`, hashes, command
transcript and `result.json`. Because the console bridge may visually wrap a
long telemetry record, analysis uses a reconstructed `perf-telemetry.log` while
the byte-for-byte collector output remains untouched.

The sample is accepted only when it contains at least two world records whose
normalized text begins with `PERF v=2 scene=world`, and both the first and last
report `thermal=nominal`.
Completed but rejected measurements exit with status 2 and keep all evidence.
Operational or restoration failures use another non-zero status. Always resolve
a restoration failure using the retained `original-device-Gothic.ini` before
starting another measurement.

Use `--dry-run` to validate the complete invocation and print its safety plan
without contacting or mutating the device.
