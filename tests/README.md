# Input candidate checks

Run the pure C++ checks with assertions enabled (do not pass `-DNDEBUG`):

```sh
clang++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icommon tests/padmovement.cpp -o /path/to/builds/padmovement
/path/to/builds/padmovement
```

The checks cover radial dead zones, diagonal normalization, activation/release
hysteresis, invalid samples, camera-relative headings, shortest-arc rotation,
large-frame clamping, turn-before-run and equivalent 30/60 Hz stepping.

`ios/RendererIOSUITests.xcodeproj` is a simulator-only gesture harness. Install a
candidate with bundle ID `opengothic.gothic2.vector-qa` and copy legally owned game
data and saves 1/4 into its isolated Documents container first. The gesture hit
points target **iPhone 16 Pro Max, landscape**. No save operation is performed.
Build products and result bundles belong under the workspace's central `builds/`
and `artifacts/qa/` directories, not this worktree.

The UI tests exercise cardinal/diagonal movement, camera drag-and-hold, release,
jump, weapon-mode movement and background/resume. Foreground assertions detect
crashes, while named screenshots require visual review of the actual loaded world
and actions. A green test alone is not proof of correct motion or button hitboxes.
It does not validate physical controllers, simultaneous fingers, thermal behavior
or gameplay responsiveness on a real phone.

`testTouchItemAssignment` exercises inventory R3, a no-selection confirm, dragged
outer selection, release followed by RT assignment, reopening, LT clearing,
cancellation, an inner-row assignment and use from the world ring.
`testGoldCannotBeAssigned` checks that gold has no editor entry point.
These change only the loaded session's layout, not the save file. Review the
named screenshots to verify the slots and that the inventory item was not used
or equipped underneath the editor.

Screenshots use `XCUIScreen.main` because the application accessibility frame can
be stale after the game forces its landscape orientation. `testCameraHoldAndRelease`
also captures the world 12 seconds after releasing a sustained right-stick input.

`testContextualAttackA` checks inventory A, draws the equipped weapon, taps A three
times and RT once, sheathes, then returns through the menu. Review its screenshots
for the attack poses; foreground assertions alone only prove the game stayed alive.

## Contextual A/RT input probe

`ios/controllerattack.cpp` tests the real `GamepadInput`, `TouchInput` and
`PlayerControl` callbacks in a paused Simulator game with no connected controller.
It covers shared A/RT ownership, both release orders, short A taps (including after
RT release), aim restoration, weapon-mode changes and input resets. The six weapon
fixtures change only in-memory mode and restore it before resuming: they test
input routing, not ammunition, damage or spell execution. No save is written.

Build it as an arm64 Simulator bundle against the candidate app, using the app's
matching compiler response file (include paths and defines):

```sh
xcrun clang++ -target arm64-apple-ios15.0-simulator \
  -isysroot "$(xcrun --sdk iphonesimulator --show-sdk-path)" \
  @/path/to/matching/common-args.resp -fno-access-control -bundle \
  -bundle_loader /path/to/Gothic2Notr.app/Gothic2Notr \
  tests/ios/controllerattack.cpp -o /path/to/builds/controllerattack.bundle
```

Launch the isolated QA app with `-nomenu -save 1`. After the world loads, attach
LLDB, break in `GamepadInput::tick`, capture `this`, and disable the breakpoint.
Load the bundle from a Simulator-readable path such as a dedicated `/tmp` folder:

```text
expression -- GamepadInput *$pad = this
expression -- void *$probe = (void*)dlopen("/path/to/controllerattack.bundle",2)
expression -- $probe
```

Only if the handle is non-null, call
`(int)((int(*)(void*))dlsym($probe,"testControllerAttack"))($pad)`.
The return value must be zero and the app console must print the final PASS count;
a nonzero return identifies the failed check. Flush stdout, detach, then terminate
the QA app. Never ship or load this probe on a phone. It supplements rather than
replaces gesture tests or validation with a real physical controller.
