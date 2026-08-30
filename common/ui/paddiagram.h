#pragma once

#include <cstdint>

namespace Tempest { class Painter; }
class GthFont;
enum class ScriptLang : int32_t;

// Full-screen controller-layout help page: the bundled Xelu pad line-art with
// leader lines, button glyphs and localized labels for this fork's gamepad
// mapping. The native iOS device-settings overlay shows it without replacing
// the MENU.DAT controls page.
namespace PadDiagram {
  // True when the bundled pad line-art is present (iOS app bundle only).
  bool available();

  // Paint the whole page into a w x h widget. The labels mirror the actual
  // bindings in GamepadInput::tickWorld - keep both in sync. contentRight
  // optionally reserves a right-hand column for a sibling settings panel.
  void draw(Tempest::Painter& p, const GthFont& fnt, int w, int h, float scale,
            ScriptLang language, bool reserveVersionLine,
            int contentRight=0);
  }
