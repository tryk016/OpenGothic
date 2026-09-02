# Android port

The Android port is developed only on the `android` branch. It is based on a
clean upstream OpenGothic snapshot; it is not merged into `master`.

Tempest Android platform changes live as normal commits on the `android`
branch of `tryk016/Tempest`. This repository points its `lib/Tempest`
submodule at a reviewed commit from that branch. The previous workflow that
rewrote a dirty Tempest checkout with `android/patches/apply-patches.sh` is not
used by this port.

## Migration gates

1. **First light (complete):** GameActivity loads `libGothic2Notr.so`, Tempest
   creates an Android Vulkan surface, and a solid dark-red frame is visible.
2. **Boot to menu (complete):** link the `OpenGothic` core, restore game-data
   discovery at `/sdcard/OpenGothic/Gothic2`, and reach the Gothic II menu.
3. **Lifecycle and input (complete):** survive background/resume with a
   recreated Vulkan surface and verify touch controls.
4. **Memory and Mali compatibility (complete):** unsupported BC textures are
   transcoded once to a persistent ASTC 4x4 cache, Android uses four engine
   workers, and bindless fallback descriptors are valid on Mali-G57. The
   Adreno 619 retest remains open because that device is not currently
   available; do not describe it as verified.
5. **Measured Helio G99 baseline (complete):** stop the Android compositor's
   recurring suboptimal-swapchain rebuilds and expose measured controls for
   shadow resolution, internal resolution, upscaling filter, and volumetric
   fog resolution. The selected profile is stable but does not yet hold
   30 FPS.
6. **Production upscaler (complete):** FSR 1 EASU, with optional RCAS, runs
   after tone mapping and before native-resolution UI composition. Bilinear
   remains the fastest fallback on entry-level GPUs.

CI migration builds are signed with the existing development key so they can
update an installed test build without deleting game data. Migration version
codes start at 100000, above the legacy repository's published builds, and add
the workflow run number so every successful CI build remains upgradeable.
CI migration builds remain workflow artifacts. This repository does not yet
publish or update a public `latest-android` release; release publishing stays
disabled until Adreno is retested and physical-device qualification is complete.

The application is fixed to landscape. A full game-data installation contains
all three original directories:

```text
/sdcard/OpenGothic/Gothic2/_work
/sdcard/OpenGothic/Gothic2/Data
/sdcard/OpenGothic/Gothic2/system
```

## Verified Helio G99 profiles

The performance profile remains:

```ini
; /sdcard/OpenGothic/Gothic.ini
[INTERNAL]
vidResIndex=2
resolutionScale=0.5
fogResolutionScale=0.5
upscaleFilter=1

[ENGINE]
shadowResolution=512
```

For a sharper image at a measured performance cost, use the FSR 1 quality
profile:

```ini
[INTERNAL]
resolutionScale=0.5
fogResolutionScale=0.5
upscaleFilter=2
fsrSharpness=0

[ENGINE]
shadowResolution=512
```

`/sdcard/OpenGothic/Gothic2/system/SystemPack.ini` also uses
`FPS_Limit=30`. The existing low-cost Android options disable SSAO,
reflections, wind, expensive water effects, and similar desktop-oriented
effects.

`resolutionScale` and `fogResolutionScale` are clamped to `0.25..1.0`.
Setting `resolutionScale=0` preserves the old `vidResIndex` mapping.
`upscaleFilter=0` selects the existing Lanczos path, `1` selects bilinear, and
`2` selects FSR 1. `fsrSharpness=0` runs EASU without an extra pass; values in
`0..1` continuously increase optional RCAS sharpening. FSR clamps internal
resolution to at least 50%, its documented maximum 4x area upscale.
`shadowResolution` is clamped to `128..4096`; the cross-platform default
remains 2048. All new defaults preserve existing Windows, Linux, macOS, and
iOS behaviour.

### Physical-device measurements

These are directional 20-second SurfaceFlinger samples from the same early
Xardas-world workload on a Samsung SM-X115 (Helio G99 / Mali-G57). They are
not a general game benchmark, but they are sufficient for accepting or
rejecting each change.

| Configuration | FPS | Decision |
| --- | ---: | --- |
| Recurring swapchain rebuilds | 8.29 | fixed |
| Swapchain fix, previous renderer profile | 12.22 | kept |
| 50% internal, Lanczos, 512 shadows | 15.26 | Lanczos too expensive |
| 50% internal, bilinear, 512 shadows | 23.38 | kept |
| 35% internal, bilinear, 512 shadows | 25.54 | rejected: large quality loss for small gain |
| 50% internal, bilinear, 50% fog LUT, 512 shadows | 24.55 | selected profile |
| 50% internal, bilinear, 25% fog LUT, 512 shadows | 24.01 | rejected: no gain |
| 50% internal, same-run bilinear control | 24.17 | fastest fallback |
| 50% internal, FSR 1 EASU, full float | 20.99 | correct image, kept as reference |
| 50% internal, FSR 1 EASU, relaxed filter precision | 21.23 | selected quality profile |
| 50% internal, FSR 1 EASU + RCAS pass | 19.89 | optional; too costly for this device |

Reducing shadows from 512 to 256 changed 15.26 FPS to only 15.48 FPS, so 512
is retained. The selected profile had a 32.75 ms median frame time and a
48.96 ms p95; it is therefore not a stable 30 FPS profile yet. The GPU remains
the limiting component, while memory and thermals stayed safe during these
short acceptance runs.

The final FSR EASU sample ran at thermal status 0, used about 1.20 GiB PSS,
and survived 5 Home/resume plus 2 screen-off/wake cycles with the same PID and
without a fatal signal, ANR, or Vulkan device loss. FSR replaces the much more
expensive Lanczos path, but it does not turn this GPU-limited scene into a
stable 30 FPS workload; bilinear remains about 3 FPS faster.

The cold ASTC pass encoded 672 textures. On a warm launch all 672 were cache
hits and none were re-encoded. The resident compressed texture payload was
about 135 MiB instead of about 541 MiB for the RGBA fallback, while the
persistent `/sdcard/OpenGothic/astc-v2` cache occupied about 164 MiB. Loaded
world samples used roughly 1.14-1.35 GiB PSS without an OOM or device loss.

### FSR 1 integration

The existing Lanczos path performs many full-resolution texture samples and
is disproportionately expensive on Mali-G57. The implemented
[AMD FidelityFX Super Resolution 1](https://gpuopen.com/fidelityfx-superresolution/)
path tone-maps the low-resolution HDR scene into a perceptual intermediate,
runs EASU to the display resolution, optionally runs RCAS, and then draws the
UI at native resolution. This keeps the integration spatial and Vulkan-friendly
without requiring motion vectors, jitter, history, depth, or temporal passes.
Arm ASR/NSS remains out of scope for this stage because those temporal inputs
would require a substantially larger renderer rewrite.

Future image-quality experiments may evaluate a negative material mip bias and
low-resolution anti-aliasing before EASU. They are deliberately separate from
this gate because both need broader sampler or render-pass changes and must be
measured for shimmer and GPU cost.

## Android lifecycle contract

Tempest keeps one process-lifetime Android window wrapper while the native
`ANativeWindow` inside it is replaced by GameActivity. `APP_CMD_TERM_WINDOW`
synchronously reports a zero-sized window before GameActivity releases the
native surface. Rendering and timers stay suspended while the app is paused or
has no native window. `APP_CMD_INIT_WINDOW` installs the replacement window and
dispatches its real size, which recreates both `VkSurfaceKHR` and the Vulkan
swapchain. Active touch state is cancelled when the app is suspended.

OpenGothic treats a zero-sized swapchain as a suspended state and does not
update camera or cursor geometry until a real surface returns. Android remains
fixed to landscape by the manifest. The Vulkan swapchain currently requests an
identity presentation transform and leaves rotation to Android; renderer-side
pre-rotation is a later optimization, not part of lifecycle correctness.

The lifecycle gate was verified on a Samsung SM-X115 (Helio G99 / Mali-G57,
Android 15): 20 Home/Recents resumes and 5 screen-off/wake/resume cycles kept
the same game process alive and returned to an upright landscape menu. The
performance candidate was additionally tested in the loaded world with 5
Home/resume and 2 screen-off/wake cycles; all seven transitions kept the same
PID and produced no fatal signal or ANR. The final device log contained no
surface-loss or device-loss entry.

The verified Tempest integration point is `b0004eef`. Android and the full
Windows, Linux, Linux package, macOS x64, and macOS ARM64 CI matrix must pass
before each renderer gate is considered closed.
