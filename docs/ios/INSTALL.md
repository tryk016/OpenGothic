# Building and installing OpenGothic on iOS

This guide covers the maintained MetalFX Temporal performance release,
SideStore/AltStore sideloading and local Xcode installation. The release uses
512 x 512 mobile shadow maps and skips full skeletal-pose work for distant,
off-screen NPCs while preserving animation time and gameplay events.
OpenGothic does not distribute the original Gothic II data. You must supply
files from a legally owned copy of *Gothic II: Night of the Raven*.

## Install the release with SideStore or AltStore

The maintained iOS release is an unsigned IPA. It is not an Apple App Store
package and cannot be launched by opening it in Files; a sideloading client
must sign it for your device first.

For SideStore (recommended):

1. Complete the one-time SideStore setup from [sidestore.io](https://sidestore.io).
2. Open **Sources**, tap **+** and add this source URL:

   `https://github.com/tryk016/OpenGothic/releases/download/ios-metalfx-temporal/apps.json`

3. Open the new OpenGothic source and install **OpenGothic MetalFX Temporal**.
4. When a newer version appears, choose **Update**. Do not uninstall the old
   version first: an update using the same bundle identifier preserves game
   data, settings and saves in `Documents`.

AltStore and other clients compatible with AltStore sources can use the same
`apps.json`. The IPA can also be downloaded directly from the
[MetalFX Temporal release](https://github.com/tryk016/OpenGothic/releases/tag/ios-metalfx-temporal)
and selected manually in the sideloading client. Free Apple-account signatures
normally need periodic refresh; follow the selected client's current setup and
refresh instructions.

After the app is installed, continue with [Copy the game data](#copy-the-game-data).
The release contains only the open-source engine, never Gothic II game assets.

## What you need to build locally

- An iPhone or iPad running iOS or iPadOS 15.0 or newer
- A Mac with a current Xcode installation and Xcode command-line tools
- CMake, `glslangValidator` and the repository's recursive Git submodules
- An Apple account and a signing team accepted by Xcode for device installation
- Your own complete Gothic II: Night of the Raven installation

The included helper uses Homebrew when one of these build tools is missing.
Install Homebrew first, or provide the required tools through another location
in `PATH`.

## Clone the repository

Until the iOS work is accepted upstream, clone the maintained integration fork
together with all pinned dependencies:

```sh
git clone --recurse-submodules --branch codex/ios-upstream-integration \
  https://github.com/tryk016/OpenGothic.git
cd OpenGothic
```

For an existing checkout, synchronize it before configuring:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

Do not replace the pinned Tempest commit with an arbitrary branch tip. The
OpenGothic revision and its submodule revisions are tested as one source tree.

## Build and install with Xcode

Run the helper from the repository root:

```sh
./ios/build-ios.sh
open build-ios/OpenGothic.xcodeproj
```

The helper configures an arm64 iOS project with a deployment target of 15.0.
In Xcode:

1. Select the `Gothic2Notr` application target.
2. Open **Signing & Capabilities**.
3. Enable automatic signing and select your development team.
4. If Xcode reports a bundle-identifier conflict, choose a unique identifier.
5. Connect and select the iPhone or iPad as the run destination.
6. Build and run. Follow any device prompts to trust the developer identity or
   enable Developer Mode.

The application is landscape-only. A physical controller is not required
because the full touch controller is included.

## Command-line and unsigned builds

An unsigned device build can be generated without changing the source tree:

```sh
cmake -S . -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0

cmake --build build-ios --config Release --target Gothic2Notr -- \
  -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY=""
```

The repository's iOS CI also builds device, simulator, performance and runtime
startup-fallback profiles. CI artifacts are unsigned validation outputs, not
the stable release channel. Use the release source above for normal
installation, or sign a CI artifact with credentials and entitlements valid
for the target device.

For an arm64 Simulator configuration, replace both occurrences of `iphoneos`
with `iphonesimulator`. Simulator artifacts are intended for development and CI
and do not replace physical-device acceptance testing.

## Optional rendering profiles

The source defaults keep optional performance features disabled so the
conservative paths remain continuously buildable. The maintained IPA enables
the recommended performance profile shown below. Individual options can also
be selected explicitly when testing:

| CMake option | Purpose |
|---|---|
| `OPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT` | Use three frame-resource and Metal swapchain slots |
| `OPENGOTHIC_IOS_DIRECT_DRAWABLE` | Prefer direct drawable rendering with copy fallback |
| `OPENGOTHIC_NPC_ANIMATION_CULLING` | Skip full skeletal-pose work for distant, off-screen NPCs while preserving animation events |
| `OPENGOTHIC_NPC_DIALOG_CULLING` | Experimental aggressive dialog/cutscene mode; not enabled in the recommended profile |
| `OPENGOTHIC_METALFX_SPATIAL` | Enable MetalFX Spatial where available |
| `OPENGOTHIC_METALFX_TEMPORAL` | Enable MetalFX Temporal; requires Spatial for fallback |
| `OPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS` | Bundle the verified startup shader set; enabled by default |

For example:

```sh
cmake -S . -B build-ios-performance -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DOPENGOTHIC_IOS_THREE_FRAMES_IN_FLIGHT=ON \
  -DOPENGOTHIC_IOS_DIRECT_DRAWABLE=ON \
  -DOPENGOTHIC_NPC_ANIMATION_CULLING=ON \
  -DOPENGOTHIC_METALFX_SPATIAL=ON \
  -DOPENGOTHIC_METALFX_TEMPORAL=ON
```

Unsupported MetalFX paths fall back at runtime. To verify the authoritative
runtime shader-compile path, configure with
`-DOPENGOTHIC_IOS_PRECOMPILED_STARTUP_SHADERS=OFF`.

## Copy the game data

Install the application first so its file-sharing container exists. Then copy
the **contents** of your Gothic II installation into OpenGothic's `Documents`
directory. The important top-level layout is:

```text
Documents/
├── Data/
├── _work/
└── system/
```

Do not add another enclosing `Gothic II` directory. For a typical Steam
installation on Windows, the source is similar to:

```text
C:\Program Files (x86)\Steam\steamapps\common\Gothic II
```

Copy the files using one of the platform file-sharing paths available to you:

- Finder on macOS: select the connected device, open **Files**, choose
  OpenGothic and add the folders;
- Apple Devices or iTunes File Sharing on Windows;
- the Files app on iOS or iPadOS, for example from iCloud Drive or external
  storage.

OpenGothic checks the working directory for `Data`, `_work/Data` and compiled
scripts. If required data is not found, the application presents an error
instead of starting a world. Correct the directory layout and launch it again.

## Configuration and saves

The installed data and the iOS override are intentionally separate:

```text
Documents/system/Gothic.ini  # copied base configuration
Documents/Gothic.ini         # writable iOS override
```

The root override is created only after game-data validation succeeds. Saves
and manually assigned Items-ring slots are stored in the app container as
normal game state.

Installing a newer build over an application with the same bundle identifier
normally retains `Documents`. Uninstalling the application removes its
container, including copied game data and saves. Back up the complete
`Documents` directory before uninstalling, changing bundle identifiers or
testing a migration.

## Troubleshooting

### Xcode cannot sign the application

Confirm that a team is selected for the `Gothic2Notr` target and that its bundle
identifier is available to that team. Automatic signing is the simplest local
configuration. An unsigned CI artifact does not carry a reusable provisioning
profile.

### The application reports missing game data

Check that `Data`, `_work` and `system` are directly inside the OpenGothic
`Documents` directory. Confirm that `_work/Data` and the compiled scripts from
the installed game language are present.

### MetalFX is unavailable

MetalFX depends on the OS, device and active texture configuration. This is not
a missing-data error. A build with the optional backends enabled automatically
uses Spatial or Lanczos when the higher-level scaler cannot be used.

### A startup shader library is rejected

The startup libraries are optional and validated before Metal uses them. A
missing or mismatched library falls back to Tempest's normal runtime compiler.
Use the startup-fallback build profile when diagnosing that path.

## Related documentation

- [iOS port overview and controls](README.md)
- [Controller maintainer reference](CONTROLLER.md)
- [Startup shader maintainer reference](SHADER-STARTUP.md)
