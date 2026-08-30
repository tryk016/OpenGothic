#include "iosuifont.h"

#include <Tempest/Brush>
#include <Tempest/Color>
#include <Tempest/Painter>
#include <Tempest/Platform>

#include <algorithm>

#include "ui/iosuilocalization.h"
#include "utils/gthfont.h"

using namespace Tempest;

IosUiFont::IosUiFont(const Font& font, const GthFont& gameFont,
                     ScriptLang language)
  :font(font), language(language) {
#if defined(__IOS__)
  this->font.setPixelSize(float(std::max(1,gameFont.pixelSize())));
#else
  this->gameFont = &gameFont;
#endif
  }

int IosUiFont::pixelSize() const {
  if(gameFont!=nullptr)
    return gameFont->pixelSize();
  return int(font.pixelSize());
  }

Size IosUiFont::textSize(std::string_view text) const {
  return textSizePrepared(prepareText(text));
  }

void IosUiFont::drawText(Painter& painter, int x, int baseline,
                         std::string_view text, float alpha) const {
  const std::string prepared = prepareText(text);
  drawTextPrepared(painter,x,baseline,prepared,alpha);
  }

std::string IosUiFont::prepareText(std::string_view text) const {
  if(gameFont!=nullptr)
    return std::string(text);
  return IosUiLocalization::uiTextUtf8(language,text);
  }

bool IosUiFont::preparedTextIsUtf8() const {
  return gameFont==nullptr;
  }

Size IosUiFont::textSizePrepared(std::string_view text) const {
  if(gameFont!=nullptr)
    return gameFont->textSize(text);
  return font.textSize(text);
  }

void IosUiFont::drawTextPrepared(Painter& painter, int x, int baseline,
                                 std::string_view text, float alpha) const {
  if(gameFont!=nullptr) {
    gameFont->drawText(painter,x,baseline,text);
    return;
    }
  painter.pushState();
  painter.setFont(font);
  painter.setBrush(Color(1.f,1.f,1.f,alpha));
  painter.drawText(x,baseline,text);
  painter.popState();
  }
