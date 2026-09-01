# Android port

The Android port is developed only on the `android` branch. It is based on a
clean upstream OpenGothic snapshot; it is not merged into `master`.

Tempest Android platform changes live as normal commits on the `android`
branch of `tryk016/Tempest`. This repository points its `lib/Tempest`
submodule at a reviewed commit from that branch. The previous workflow that
rewrote a dirty Tempest checkout with `android/patches/apply-patches.sh` is not
used by this port.

## Migration gates

1. **First light:** GameActivity loads `libGothic2Notr.so`, Tempest creates an
   Android Vulkan surface, and a solid dark-red frame is visible.
2. **Boot to menu:** link the `OpenGothic` core, restore game-data discovery at
   `/sdcard/OpenGothic/Gothic2`, and reach the Gothic II menu.
3. **Lifecycle and input:** survive background/resume with a recreated Vulkan
   surface and verify touch controls.
4. **Memory and compatibility:** restore the ASTC cache, validate Mali-G57,
   and retest the isolated Adreno 619 driver failure.
5. **Performance:** migrate only optimizations that still pass measurements on
   the current renderer; do not replay historical probes or reverted patches.

CI migration builds are signed with the existing development key so they can
update an installed test build without deleting game data. Until boot-to-menu
and lifecycle parity pass on a device, CI uploads APKs only as workflow
artifacts and does not update the public `latest-android` release.

The application is fixed to landscape. A full game-data installation contains
all three original directories:

```text
/sdcard/OpenGothic/Gothic2/_work
/sdcard/OpenGothic/Gothic2/Data
/sdcard/OpenGothic/Gothic2/system
```
