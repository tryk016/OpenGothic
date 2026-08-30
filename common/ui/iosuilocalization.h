#pragma once

#include <cstdint>
#include <string>
#include <string_view>

enum class ScriptLang : int32_t;

// Text owned by the native iOS overlays. These strings deliberately do not
// come from MENU.DAT: stock and modded menu instances must remain untouched.
namespace IosUiLocalization {

// Persistent ENGINE/iOSUiLanguage value. Values 0..7 intentionally match
// ScriptLang; Automatic keeps GAME/language authoritative when it is valid.
constexpr int AutomaticLanguage = -1;

struct DeviceSettingsText {
  const char* title;
  const char* frameRate;
  const char* off;
  const char* controlled;
  const char* back;
  };

struct PadDiagramText {
  const char* title;
  const char* ltAction;
  const char* lbAction;
  const char* move;
  const char* sneak;
  const char* itemRing;
  const char* statusPrev;
  const char* questNext;
  const char* weaponMagicRing;
  const char* rtAction;
  const char* rbAction;
  const char* weapon;
  const char* special;
  const char* jump;
  const char* action;
  const char* camera;
  const char* targetLock;
  const char* inventory;
  const char* gameMenu;
  };

// Pure helpers kept public so locale parsing and the AUTO decision can be
// covered without UIKit/CoreFoundation or an initialized Gothic singleton.
bool       isSupportedLanguage(int value);
ScriptLang languageFromIdentifier(std::string_view identifier);
ScriptLang resolveLanguage(int gameLanguage, int uiLanguage,
                           ScriptLang preferredLanguage);
const char* languageCode(ScriptLang language);

// Convert a catalog string from the language's explicit Windows codepage to
// UTF-8 for Tempest's language-independent UI font. Conversion failure uses a
// deterministic printable-ASCII fallback rather than rendering wrong glyphs.
std::string uiTextUtf8(ScriptLang language, std::string_view text);

// Read/write the native overlay choice. The setter persists immediately.
int        languageSelection();
void       setLanguageSelection(int value);

// Resolve the current native UI language. AUTO uses a valid GAME/language,
// then (only for GAME/language=-1) the preferred iOS language, then English.
ScriptLang currentLanguage();

const DeviceSettingsText& deviceSettings(ScriptLang language);
const PadDiagramText&      padDiagram(ScriptLang language);

}
