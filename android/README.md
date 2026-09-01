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
4. **Memory and compatibility (next):** restore the ASTC cache, validate
   Mali-G57, and retest the isolated Adreno 619 driver failure.
5. **Performance:** migrate only optimizations that still pass measurements on
   the current renderer; do not replay historical probes or reverted patches.

CI migration builds are signed with the existing development key so they can
update an installed test build without deleting game data. Migration version
codes start at 100000, above the legacy repository's published builds, and add
the workflow run number so every successful CI build remains upgradeable.
CI migration builds remain workflow artifacts. This repository does not yet
publish or update a public `latest-android` release; release publishing stays
disabled until the memory, compatibility, and performance gates are complete.

The application is fixed to landscape. A full game-data installation contains
all three original directories:

```text
/sdcard/OpenGothic/Gothic2/_work
/sdcard/OpenGothic/Gothic2/Data
/sdcard/OpenGothic/Gothic2/system
```

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
final device log contained no surface-loss, device-loss, fatal-signal, or ANR
entry. Tempest's Windows, MinGW, Ubuntu, and macOS CI jobs also passed. The
verified integration points are OpenGothic `ea846662` and Tempest `cf205bc7`.
