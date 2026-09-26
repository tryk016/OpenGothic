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

Screenshots use `XCUIScreen.main` because the application accessibility frame can
be stale after the game forces its landscape orientation. `testCameraHoldAndRelease`
also captures the world 12 seconds after releasing a sustained right-stick input.
