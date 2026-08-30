#pragma once

#include <Tempest/Font>
#include <Tempest/Size>

#include <cstdint>
#include <string>
#include <string_view>

namespace Tempest { class Painter; }
class GthFont;
enum class ScriptLang : int32_t;

// On iOS, localized strings are converted from their explicit catalog
// codepage to UTF-8 and drawn with Tempest's bundled Roboto font; GthFont is
// then used only by callers drawing ASCII pad glyphs. Other platforms retain
// their previous GthFont path unchanged.
class IosUiFont final {
  public:
    IosUiFont(const Tempest::Font& font, const GthFont& gameFont,
              ScriptLang language);

    int           pixelSize() const;
    Tempest::Size textSize(std::string_view text) const;
    void          drawText(Tempest::Painter& painter, int x, int baseline,
                           std::string_view text, float alpha=1.f) const;

    // Variants for layout code which prepares a catalog string once and then
    // measures its substrings repeatedly while wrapping.
    std::string   prepareText(std::string_view text) const;
    bool          preparedTextIsUtf8() const;
    Tempest::Size textSizePrepared(std::string_view text) const;
    void          drawTextPrepared(Tempest::Painter& painter, int x,
                                   int baseline, std::string_view text,
                                   float alpha=1.f) const;

  private:
    Tempest::Font font;
    const GthFont* gameFont = nullptr;
    ScriptLang    language;
  };
