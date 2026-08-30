#include "devicesettings.h"

#if defined(__IOS__)

#include <Tempest/Color>
#include <Tempest/Event>
#include <Tempest/Painter>

#include <algorithm>
#include <iterator>

#include "game/constants.h"
#include "gothic.h"
#include "mainwindow.h"
#include "resources.h"
#include "ui/iosuifont.h"
#include "ui/iosuilocalization.h"
#include "ui/paddiagram.h"
#include "utils/safearea.h"
#include "utils/gthfont.h"
#include "utils/string_frm.h"

using namespace Tempest;

namespace {
constexpr int rates[] = {0,30,60};
constexpr int languages[] = {
  IosUiLocalization::AutomaticLanguage,
  int(ScriptLang::EN), int(ScriptLang::DE), int(ScriptLang::PL),
  int(ScriptLang::RU), int(ScriptLang::FR), int(ScriptLang::ES),
  int(ScriptLang::IT), int(ScriptLang::CZ),
  };
}

DeviceSettings::DeviceSettings(MainWindow& owner) : owner(owner) {
  setCursorShape(CursorShape::Hidden);
  }

bool DeviceSettings::isOpen() const {
  return active;
  }

void DeviceSettings::open() {
  if(active)
    return;
  active = true;
  selectedRow = Row::FrameRate;
  setFocus(true);
  update();
  }

void DeviceSettings::close() {
  if(!active)
    return;
  active = false;
  owner.setFocus(true);
  update();
  }

bool DeviceSettings::Box::contains(int px, int py) const {
  return w>0 && h>0 && px>=x && px<x+w && py>=y && py<y+h;
  }

DeviceSettings::Layout DeviceSettings::layout() const {
  Layout ret;
  const SafeArea::Insets in = SafeArea::insets();
  const int safeLeft   = std::clamp(in.left,0,w());
  const int safeTop    = std::clamp(in.top,0,h());
  const int safeRight  = std::clamp(w()-std::max(0,in.right),safeLeft,w());
  const int safeBottom = std::clamp(h()-std::max(0,in.bottom),safeTop,h());
  const int safeW      = safeRight-safeLeft;
  const int safeH      = safeBottom-safeTop;
  if(safeW<=0 || safeH<=0)
    return ret;

  const int marginX = std::min(16,std::max(0,safeW/12));
  const int marginY = std::min(16,std::max(0,safeH/12));
  const int availableW = std::max(0,safeW-2*marginX);
  const int availableH = std::max(0,safeH-2*marginY);
  const int desiredW   = std::max(280,int(float(safeW)*0.58f));
  const int desiredH   = std::max(220,int(float(safeH)*0.30f));
  ret.panel.w = std::min(availableW,desiredW);
  ret.panel.h = std::min(availableH,desiredH);
  ret.panel.x = safeLeft+(safeW-ret.panel.w)/2;
  ret.panel.y = safeBottom-marginY-ret.panel.h;

  ret.gap = std::min(std::max(4,ret.panel.w/70),
                     std::max(0,ret.panel.w/8));
  const int innerX = ret.panel.x+ret.gap;
  const int innerW = std::max(0,ret.panel.w-2*ret.gap);
  const int innerRight = innerX+innerW;

  ret.titleBaseline      = ret.panel.y+13*ret.panel.h/100;
  ret.frameRateBaseline  = ret.panel.y+27*ret.panel.h/100;
  ret.controlledBaseline = ret.panel.y+58*ret.panel.h/100;
  ret.languageBaseline   = ret.panel.y+65*ret.panel.h/100;
  ret.footerBaseline     = ret.panel.y+96*ret.panel.h/100;

  ret.frameRateRow = {innerX,ret.panel.y+19*ret.panel.h/100,
                      innerW,42*ret.panel.h/100};
  const int fpsY = ret.panel.y+30*ret.panel.h/100;
  const int fpsH = std::max(28,18*ret.panel.h/100);
  const int buttonsW = std::max(0,innerW-2*ret.gap);
  const int fpsW = buttonsW/3;
  for(size_t i=0; i<ret.frameRateButtons.size(); ++i) {
    const int x = innerX+int(i)*(fpsW+ret.gap);
    const int bw = i+1==ret.frameRateButtons.size() ?
                   std::max(0,innerRight-x) : fpsW;
    ret.frameRateButtons[i] = {x,fpsY,bw,fpsH};
    }

  ret.languageRow = {innerX,ret.panel.y+61*ret.panel.h/100,
                     innerW,29*ret.panel.h/100};
  const int languageY = ret.panel.y+69*ret.panel.h/100;
  const int languageH = std::max(28,16*ret.panel.h/100);
  ret.languageButton = {innerX,languageY,innerW,
                        std::min(languageH,std::max(0,ret.panel.y+ret.panel.h-ret.gap-languageY))};
  return ret;
  }

bool DeviceSettings::isFrameRateLocked() const {
  return Gothic::options().fpsLimit>0;
  }

int DeviceSettings::frameRate() const {
  if(isFrameRateLocked())
    return Gothic::options().fpsLimit;
  const int fps = Gothic::settingsGetI("ENGINE","zMaxFps");
  for(const int value:rates)
    if(fps==value)
      return fps;
  return 30;
  }

void DeviceSettings::setFrameRate(int value) {
  if(isFrameRateLocked())
    return;
  if(value!=0 && value!=30 && value!=60)
    value = 30;
  Gothic::settingsSetI("ENGINE","zMaxFps",value);
  Gothic::flushSettings();
  update();
  }

void DeviceSettings::cycleFrameRate(int direction) {
  if(isFrameRateLocked() || direction==0)
    return;
  int index = 1;
  const int current = frameRate();
  for(size_t i=0; i<std::size(rates); ++i)
    if(rates[i]==current) {
      index = int(i);
      break;
      }
  index = (index+direction+int(std::size(rates))) % int(std::size(rates));
  setFrameRate(rates[index]);
  }

void DeviceSettings::cycleLanguage(int direction) {
  if(direction==0)
    return;
  int index = 0;
  const int current = IosUiLocalization::languageSelection();
  for(size_t i=0; i<std::size(languages); ++i)
    if(languages[i]==current) {
      index = int(i);
      break;
      }
  index = (index+direction+int(std::size(languages))) % int(std::size(languages));
  IosUiLocalization::setLanguageSelection(languages[index]);
  update();
  }

void DeviceSettings::cycleActiveRow(int direction) {
  if(selectedRow==Row::FrameRate)
    cycleFrameRate(direction);
  else
    cycleLanguage(direction);
  }

void DeviceSettings::keyDownEvent(KeyEvent& event) {
  if(!active) {
    event.ignore();
    return;
    }
  switch(event.key) {
    case Event::K_ESCAPE:
    case Event::K_B:
      close();
      break;
    case Event::K_Up:
    case Event::K_W:
      selectedRow = Row::FrameRate;
      update();
      break;
    case Event::K_Down:
    case Event::K_S:
      selectedRow = Row::Language;
      update();
      break;
    case Event::K_Left:
    case Event::K_A:
      cycleActiveRow(-1);
      break;
    case Event::K_Right:
    case Event::K_D:
    case Event::K_Return:
      cycleActiveRow(1);
      break;
    default:
      break;
    }
  event.accept();
  }

void DeviceSettings::keyUpEvent(KeyEvent& event) {
  // Consume the matching key-up even after Escape closed the overlay; otherwise
  // MainWindow would forward that release to the menu below it.
  event.accept();
  }

void DeviceSettings::mouseDownEvent(MouseEvent& event) {
  if(!active) {
    event.ignore();
    return;
    }
  const Layout geometry = layout();
  const Point pos = event.pos();
  if(!isFrameRateLocked()) {
    for(size_t i=0; i<std::size(rates); ++i)
      if(geometry.frameRateButtons[i].contains(pos.x,pos.y)) {
        selectedRow = Row::FrameRate;
        setFrameRate(rates[i]);
        event.accept();
        return;
        }
    }
  if(geometry.languageButton.contains(pos.x,pos.y)) {
    selectedRow = Row::Language;
    cycleLanguage(1);
    event.accept();
    return;
    }
  if(geometry.frameRateRow.contains(pos.x,pos.y)) {
    selectedRow = Row::FrameRate;
    update();
    event.accept();
    return;
    }
  if(geometry.languageRow.contains(pos.x,pos.y)) {
    selectedRow = Row::Language;
    update();
    event.accept();
    return;
    }
  if(geometry.panel.contains(pos.x,pos.y)) {
    event.accept();
    return;
    }
  close();
  event.accept();
  }

void DeviceSettings::paintEvent(PaintEvent& event) {
  if(!active)
    return;

  Painter p(event);
  const float scale = Gothic::interfaceScale(this);
  auto& gthFont = Resources::font(scale);
  const ScriptLang language = IosUiLocalization::currentLanguage();
  const auto& txt = IosUiLocalization::deviceSettings(language);

  // Keep the complete mapping in its own native layer, never in Controls.
  PadDiagram::draw(p,gthFont,w(),h(),scale,language,false);
  const IosUiFont uiFont(p.font(),gthFont,language);

  const Layout geometry = layout();
  if(geometry.panel.w<=0 || geometry.panel.h<=0)
    return;

  p.setBrush(Color(0.06f,0.06f,0.08f,0.93f));
  p.drawRect(geometry.panel.x,geometry.panel.y,geometry.panel.w,geometry.panel.h);
  const Box& selected = selectedRow==Row::FrameRate ?
                        geometry.frameRateRow : geometry.languageRow;
  p.setBrush(Color(0.22f,0.17f,0.08f,0.52f));
  p.drawRect(selected.x,selected.y,selected.w,selected.h);
  uiFont.drawText(p,geometry.panel.x+geometry.gap,geometry.titleBaseline,txt.title);
  uiFont.drawText(p,geometry.panel.x+geometry.gap,geometry.frameRateBaseline,txt.frameRate);

  const bool locked = isFrameRateLocked();
  const int  current = frameRate();
  for(size_t i=0; i<std::size(rates); ++i) {
    const Box& button = geometry.frameRateButtons[i];
    const bool selected = current==rates[i];
    const Color color = locked ? Color(0.20f,0.20f,0.22f,0.85f) :
                        selected ? Color(0.64f,0.48f,0.18f,0.95f) :
                                   Color(0.20f,0.20f,0.24f,0.95f);
    p.setBrush(color);
    p.drawRect(button.x,button.y,button.w,button.h);
    const char* label = rates[i]==0 ? txt.off : (rates[i]==30 ? "30" : "60");
    const auto sz = uiFont.textSize(label);
    uiFont.drawText(p,button.x+(button.w-sz.w)/2,
                    button.y+(button.h+sz.h)/2,label);
    }
  if(locked) {
    string_frm message(txt.controlled," (",current," FPS)");
    uiFont.drawText(p,geometry.panel.x+geometry.gap,
                    geometry.controlledBaseline,message);
    }

  uiFont.drawText(p,geometry.panel.x+geometry.gap,geometry.languageBaseline,"UI");
  const int selection = IosUiLocalization::languageSelection();
  string_frm languageLabel;
  if(selection==IosUiLocalization::AutomaticLanguage)
    languageLabel = string_frm("AUTO (",IosUiLocalization::languageCode(language),")");
  else
    languageLabel = IosUiLocalization::languageCode(ScriptLang(selection));
  p.setBrush(selectedRow==Row::Language ? Color(0.64f,0.48f,0.18f,0.95f) :
                                          Color(0.20f,0.20f,0.24f,0.95f));
  p.drawRect(geometry.languageButton.x,geometry.languageButton.y,
             geometry.languageButton.w,geometry.languageButton.h);
  const auto languageSize = uiFont.textSize(languageLabel);
  uiFont.drawText(p,geometry.languageButton.x+(geometry.languageButton.w-languageSize.w)/2,
                  geometry.languageButton.y+(geometry.languageButton.h+languageSize.h)/2,
                  languageLabel);

  const auto backSize = uiFont.textSize(txt.back);
  uiFont.drawText(p,geometry.panel.x+geometry.panel.w-geometry.gap-backSize.w,
                  geometry.footerBaseline,txt.back);
  }

#endif
