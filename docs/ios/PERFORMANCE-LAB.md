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
```

Selectors:

- `workerParticipants`: `0` for automatic, or `1`, `2`, `3`, `4`, `6` total
  participants including the main thread.
- `skyLutInterval`: update every `1`, `2`, `4`, or `6` rendered frames.
- `fogLutProfile`: `0` = 160x90x64, `1` = 120x68x48,
  `2` = 96x54x32. It affects only the low-quality volumetric path.
- `worldFarPlane`: `100000`, `80000`, or `60000` Gothic units.
- `waterReflectionMode`: `0` = current masked full-screen pass,
  `1` = disabled control, `2` is reserved for the water-geometry prototype.

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
