#include "touchinput.h"

#include <Tempest/Platform>

// The on-screen pad exists only on mobile: MainWindow instantiates it (and
// defines the pad* bridges it calls) under __MOBILE_PLATFORM__ only, so
// desktop builds compile this TU empty.
#if defined(__MOBILE_PLATFORM__)

#include <Tempest/Painter>
#include <Tempest/Event>
#include <Tempest/Application>
#include <algorithm>

#include "game/playercontrol.h"
#include "utils/gamepad.h"
#include "utils/gthfont.h"
#include "ui/padglyph.h"
#include "world/objects/npc.h"
#include "resources.h"
#include "mainwindow.h"
#include "gothic.h"
#include "utils/safearea.h"

using namespace Tempest;
using A = KeyCodec::Action;
using M = KeyCodec::Mapping;

static PadGlyph::Btn glyphOfKey(Tempest::Event::KeyType k) {
  switch(k) {
    case Tempest::Event::K_Up:     return PadGlyph::DPadUp;
    case Tempest::Event::K_Down:   return PadGlyph::DPadDown;
    case Tempest::Event::K_Left:   return PadGlyph::DPadLeft;
    case Tempest::Event::K_Right:  return PadGlyph::DPadRight;
    case Tempest::Event::K_Return: return PadGlyph::A;
    case Tempest::Event::K_ESCAPE: return PadGlyph::B;
    default:                       return PadGlyph::A;
    }
  }

TouchInput::TouchInput(MainWindow& owner, PlayerControl& ctrl)
  : owner(owner), ctrl(ctrl) {
  }

namespace {
struct TouchBounds {
  int left=0, top=0, right=0, bottom=0;
  };

TouchBounds touchBounds(int w, int h) {
#if defined(__IOS__)
  const auto in = SafeArea::insets();
  const int l = std::clamp(in.left,0,w);
  const int t = std::clamp(in.top,0,h);
  return {l,t,std::clamp(w-in.right,l,w),std::clamp(h-in.bottom,t,h)};
#else
  return {0,0,w,h};
#endif
  }

template<class Rect>
bool contains(const Rect& r, const Point& p) {
  return p.x>=r.x && p.x<r.x+r.w && p.y>=r.y && p.y<r.y+r.h;
  }
}

TouchInput::WorldLayout TouchInput::worldLayout() const {
  const int W = w(), H = h();
#if defined(__IOS__)
  const auto cb = touchBounds(W,H);
  const int cw = std::max(1,cb.right-cb.left);
  const int ch = std::max(1,cb.bottom-cb.top);
  const int s  = std::max(24,ch/10);
  const int m  = std::max(6,ch/45);
  const int g  = std::max(4,ch/70);
  const int step = s+g;

  const int faceCx = cb.right-m-s/2-step;
  const int faceCy = cb.bottom-m-s/2-step;
  // Width also participates so iPad's 4:3 landscape does not let two sticks
  // crowd the D-pad and face cluster. Phones still get a roughly 30%-height
  // stick, while tablets use a balanced ~1/6-content-width diameter.
  const int stick  = std::max(2*s,std::min(3*s,cw/6));
  const int faceLeft = faceCx-step-s/2;
  const int rightStickX = std::max(cb.left+cw/2,faceLeft-2*g-stick);
  const int stickY = cb.bottom-m-stick;
  const int desiredDcx = cb.left+int(float(cw)*0.43f);
  const int minDcx = cb.left+m+stick+g+step;
  const int maxDcx = rightStickX-g-step-s;
  const int dcx = minDcx<=maxDcx ? std::clamp(desiredDcx,minDcx,maxDcx) : desiredDcx;
  const int dcy = cb.bottom-m-s-step;
  const int row = cb.top+m;
  const int center = (cb.left+cb.right)/2;
  namespace G = PadGlyph;
  using     K = TAct;
  WorldLayout ret{{{
    // Equal-size face buttons in a diamond, clear of the full-size right stick.
    { faceCx-s/2,      faceCy+step-s/2, s, G::A, K::Interact, A::ActionGeneric },
    { faceCx+step-s/2, faceCy-s/2,      s, G::B, K::Special,  A::PadSpecial    },
    { faceCx-step-s/2, faceCy-s/2,      s, G::X, K::Key,      A::Jump          },
    { faceCx-s/2,      faceCy-step-s/2, s, G::Y, K::Key,      A::Weapon        },
    // Shoulders and triggers follow the reference's horizontal top rows.
    { cb.right-m-s,          row, s, G::RT, K::Rt, A::PadAttack       },
    { cb.right-m-2*s-2*g,    row, s, G::RB, K::Rb, A::PadAttackRight },
    { cb.left+m,             row, s, G::LT, K::Lt, A::PadAim          },
    { cb.left+m+s+2*g,       row, s, G::LB, K::Lb, A::PadAttackLeft   },
    // Stick clicks.
    { cb.left+m, cb.top+ch/2-s/2, s, G::L3, K::Key,  A::Sneak         },
    { rightStickX+(stick-s)/2, cb.top+ch/2-s/2, s, G::R3, K::Lock, A::ActionGeneric },
    // D-pad and ring/focus actions.
    { dcx,      dcy-step, s, G::DPadUp,    K::ItemRing,      A::Idle },
    { dcx,      dcy+step, s, G::DPadDown,  K::WeaponsRing,   A::Idle },
    { dcx-step, dcy,      s, G::DPadLeft,  K::JournalOrFocus,A::Idle },
    { dcx+step, dcy,      s, G::DPadRight, K::MapOrFocus,    A::Idle },
    // View/menu remain centred and clear of the Dynamic Island safe area.
    { center-(s+g), row, s, G::View, K::SystemView, A::Inventory },
    { center+g,     row, s, G::Menu, K::SystemMenu, A::Escape    },
  }},
  {cb.left+m,stickY,stick,stick},
  {rightStickX,stickY,stick,stick}};
  return ret;
#else
  const int s  = H/11;
  const int m  = H/40;
  const int bx = W-s-m, by = H-s-m;                 // face cluster anchor (bottom-right)
  const int tR = W-s-m, tL = m;                     // shoulder columns
  const int row0 = m, row1 = m+s+m;
  const int dcx = int(float(W)*0.44f), dcy = H-int(float(s)*1.7f)-m;   // d-pad centre
  namespace G = PadGlyph;
  using     K = TAct;
  return {{{
    // face
    { bx,        by,        s, G::A, K::Interact, A::ActionGeneric },
    { bx,        by-(s+m),  s, G::B, K::Special,  A::PadSpecial    },
    { bx-(s+m),  by,        s, G::X, K::Key,      A::Jump          },
    { bx-(s+m),  by-(s+m),  s, G::Y, K::Key, A::Weapon        },
    // shoulders / triggers
    { tR,        row0,      s, G::RT, K::Rt, A::PadAttack },
    { tR,        row1,      s, G::RB, K::Rb, A::PadAttackRight },
    { tL,        row0,      s, G::LT, K::Lt, A::PadAim },
    { tL,        row1,      s, G::LB, K::Lb, A::PadAttackLeft },
    // stick clicks
    { m,         H-H/3-m-s-m, s, G::L3, K::Key,  A::Sneak         },
    { bx,        by-2*(s+m),  s, G::R3, K::Lock, A::ActionGeneric },
    // d-pad: two separate rings plus journal/map or target navigation
    { dcx,       dcy-s, s, G::DPadUp,    K::ItemRing,   A::Idle },
    { dcx,       dcy+s, s, G::DPadDown,  K::WeaponsRing,A::Idle },
    { dcx-s,     dcy,   s, G::DPadLeft,  K::JournalOrFocus,A::Idle },
    { dcx+s,     dcy,   s, G::DPadRight, K::MapOrFocus, A::Idle },
    // system
    { W/2-(s+m), m, s, G::View, K::SystemView, A::Inventory },
    { W/2+m,     m, s, G::Menu, K::SystemMenu, A::Escape    },
  }},
  {H/20,H-H/3-H/20,H/3,H/3},
  {W/2+1,0,std::max(0,W-(W/2+1)),H}};
#endif
  }

std::array<TouchInput::PadArea,3> TouchInput::ringControls() const {
  const auto cb = touchBounds(w(),h());
  const int ch = std::max(1,cb.bottom-cb.top);
  const int s = ch/10;
  const int m = ch/40;
  return {{
    {cb.left+m,cb.bottom-2*s-2*m,s,s},
    {cb.left+m,cb.bottom-s-m,    s,s},
    {cb.right-s-m,cb.bottom-s-m, s,s},
  }};
  }

std::array<TouchInput::MBtn,6> TouchInput::menuLayout() const {
  const int W = w(), H = h();
#if defined(__IOS__)
  const auto cb = touchBounds(W,H);
  const int ch = std::max(1,cb.bottom-cb.top);
  const int s  = ch/9;
  const int m  = ch/40;
  using E = Tempest::Event;
  const int cx = cb.left+m+s;
  const int by = cb.bottom-m;
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up     },
    { cx,          by-s,        s, E::K_Down   },
    { cx-(s+m),    by-2*s-m,    s, E::K_Left   },
    { cx+(s+m),    by-2*s-m,    s, E::K_Right  },
    { cb.right-2*(s+m), by-s,   s, E::K_Return },
    { cb.right-(s+m),   by-s,   s, E::K_ESCAPE },
  }};
#else
  const int s  = H/9;
  const int m  = H/40;
  using E = Tempest::Event;
  const int cx = m + s;
  const int by = H - m;
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up     },
    { cx,          by-s,        s, E::K_Down   },
    { cx-(s+m),    by-2*s-m,    s, E::K_Left   },
    { cx+(s+m),    by-2*s-m,    s, E::K_Right  },
    { W-2*(s+m),   by-s,        s, E::K_Return },
    { W-(s+m),     by-s,        s, E::K_ESCAPE },
  }};
#endif
  }

std::array<TouchInput::MBtn,4> TouchInput::dialogLayout() const {
  const int W = w(), H = h();
#if defined(__IOS__)
  const auto cb = touchBounds(W,H);
  const int ch = std::max(1,cb.bottom-cb.top);
  const int s  = ch/9;
  const int m  = ch/40;
  using E = Tempest::Event;
  const int cx = cb.left+m+s;
  const int by = cb.bottom-m;
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up     },
    { cx,          by-s,        s, E::K_Down   },
    { cb.right-2*(s+m), by-s,   s, E::K_Return },
    { cb.right-(s+m),   by-s,   s, E::K_ESCAPE },
  }};
#else
  const int s  = H/9;
  const int m  = H/40;
  using E = Tempest::Event;
  const int cx = m + s;
  const int by = H - m;
  return {{
    { cx,          by-3*s-2*m,  s, E::K_Up     },
    { cx,          by-s,        s, E::K_Down   },
    { W-2*(s+m),   by-s,        s, E::K_Return },
    { W-(s+m),     by-s,        s, E::K_ESCAPE },
  }};
#endif
  }

std::array<TouchInput::PageBtn,2> TouchInput::characterPageLayout() const {
#if defined(__IOS__)
  const auto cb = touchBounds(w(),h());
  const int ch = std::max(1,cb.bottom-cb.top);
  const int s = ch/9, m = ch/40;
  const int cx = (cb.left+cb.right)/2;
  return {{
    { cx-s-m, cb.top+m, s, PadGlyph::LB, -1 },
    { cx+m,   cb.top+m, s, PadGlyph::RB,  1 },
  }};
#else
  const int s = h()/9, m = h()/40;
  return {{
    { w()/2-s-m, m, s, PadGlyph::LB, -1 },
    { w()/2+m,   m, s, PadGlyph::RB,  1 },
  }};
#endif
  }

#if defined(__IOS__)
TouchInput::DeviceBtn TouchInput::deviceButton() const {
  const auto cb = touchBounds(w(),h());
  const int ch = std::max(1,cb.bottom-cb.top);
  const int s = ch/11;
  const int m = ch/40;
  // Menu navigation is anchored at the bottom; this leaves it and the
  // character-page LB/RB controls untouched.
  return {cb.right-s-m,cb.top+m,s};
  }
#endif

void TouchInput::aimRing(const Point& pos) {
  const float R  = float(std::min(w(),h()))/3.f;
  const float nx = float(pos.x - w()/2)/R;
  const float ny = float(h()/2 - pos.y)/R;      // up positive, matching the stick
  owner.padRingAim(nx, ny);
  }

void TouchInput::releaseWorldTouches() {
  if(mv[0]) ctrl.onKeyReleased(A::Forward, M::Primary);
  if(mv[1]) ctrl.onKeyReleased(A::Back,    M::Primary);
  if(mv[2]) ctrl.onKeyReleased(A::RotateL, M::Primary);
  if(mv[3]) ctrl.onKeyReleased(A::RotateR, M::Primary);
  mv[0]=mv[1]=mv[2]=mv[3]=false;
  moveId = -1;
  lookId = -1;
  ringId = -1;
  if(walkId>=0)
    ctrl.setGamepadWalk(false);
  walkId = -1;
  viewId = -1;
  menuId = -1;
  systemGesture.reset();

  for(auto& held : btnDown)
    ctrl.onKeyReleased(held.second, M::Primary);
  btnDown.clear();
  }

bool TouchInput::dispatchSystemEffect(PadSystemGesture::Effect effect) {
  using E = PadSystemGesture::Effect;
  if(effect==E::None)
    return false;

  // Suppress every touch still down before an effect can switch UI context.
  systemGesture.reset(viewId>=0,menuId>=0);
  if(owner.padContext()!=PadCtx::World)
    return true;
  switch(effect) {
    case E::Inventory:  owner.uiAction(A::Inventory);   break;
    case E::GameMenu:   owner.uiAction(A::Escape);      break;
    case E::None:                                        break;
    }
  return true;
  }

void TouchInput::tick() {
  if(Gamepad::poll().connected || owner.padContext()!=PadCtx::World) {
    releaseWorldTouches();
    return;
    }
  dispatchSystemEffect(systemGesture.tick(Application::tickCount()));
  }

void TouchInput::paintEvent(PaintEvent& e) {
  if(owner.padRingOpen()) {
    // TouchInput is the last widget in MainWindow's stack, so this keeps the
    // radial above inventory/menu widgets for both touch and physical pads.
    owner.padPaintRing(e);
    if(Gamepad::poll().connected)
      return;
    // Keep the full overlay off the radial sectors, but retain the three
    // modal controls in the empty corners: panel switch and explicit cancel.
    Painter p(e);
    auto& fnt = Resources::font(Gothic::interfaceScale(this));
    const auto c = ringControls();
    PadGlyph::draw(p,fnt,PadGlyph::DPadUp,  c[0].x,c[0].y,c[0].w);
    PadGlyph::draw(p,fnt,PadGlyph::DPadDown,c[1].x,c[1].y,c[1].w);
    PadGlyph::draw(p,fnt,PadGlyph::B,       c[2].x,c[2].y,c[2].w);
    return;
    }
  if(Gamepad::poll().connected)
    return;                          // a gamepad drives the UI -> hide the touch overlay

  Painter p(e);
  auto&   fnt = Resources::font(Gothic::interfaceScale(this));

  if(owner.padVideoActive()) {
    // A bink is playing: don't cover it with menu buttons, just hint that a tap skips it.
    const char* hint = "Tap to skip";
    const auto  ts   = fnt.textSize(hint);
    const auto cb = touchBounds(w(),h());
    const int m = std::max(1,(cb.bottom-cb.top)/20);
    fnt.drawText(p,cb.right-ts.w-m,cb.bottom-m,hint);
    return;
    }

  switch(owner.padContext()) {
    case PadCtx::World: {
      const auto wl = worldLayout();
#if defined(__IOS__)
      PadGlyph::drawTouch(p,fnt,PadGlyph::LStick,wl.move.x,wl.move.y,wl.move.w,0.72f);
      PadGlyph::drawTouch(p,fnt,PadGlyph::RStick,wl.look.x,wl.look.y,wl.look.w,0.72f);
      for(auto& b:wl.buttons)
        PadGlyph::drawTouch(p,fnt,b.glyph,b.x,b.y,b.s,0.76f);
#else
      PadGlyph::draw(p,fnt,PadGlyph::LStick,wl.move.x,wl.move.y,wl.move.w,0.7f);
      for(auto& b:wl.buttons)
        PadGlyph::draw(p, fnt, b.glyph, b.x, b.y, b.s);
#endif
      break;
      }
    case PadCtx::Dialog:
      for(auto& b:dialogLayout())
        PadGlyph::draw(p, fnt, glyphOfKey(b.key), b.x, b.y, b.s);
      break;
    case PadCtx::Menu:
      for(auto& b:menuLayout())
        PadGlyph::draw(p, fnt, glyphOfKey(b.key), b.x, b.y, b.s);
#if defined(__IOS__)
      if(owner.canOpenDeviceSettings()) {
        const auto b = deviceButton();
        PadGlyph::draw(p,fnt,PadGlyph::Y,b.x,b.y,b.s);
        }
#endif
      if(owner.padCharacterPageActive())
        for(auto& b:characterPageLayout())
          PadGlyph::draw(p, fnt, b.glyph, b.x, b.y, b.s);
      break;
    case PadCtx::Inventory: {
      for(auto& b:menuLayout())
        PadGlyph::draw(p, fnt, glyphOfKey(b.key), b.x, b.y, b.s);
      for(auto& b:characterPageLayout())
        PadGlyph::draw(p,fnt,b.glyph,b.x,b.y,b.s);
      break;
      }
    case PadCtx::Loading:
      break;
    }
  }

void TouchInput::mouseDownEvent(MouseEvent& e) {
  if(Gamepad::poll().connected) {
    releaseWorldTouches();
    // A controller-driven ring is modal above InventoryMenu. Keep the event
    // accepted here; ignore() would forward the tap to the highlighted item
    // underneath the assignment editor and use/equip it accidentally.
    if(owner.padRingOpen())
      return;
    e.ignore();
    return;
    }   // gamepad active -> ignore taps

  const Point  pos = e.pos();
  const int    id  = e.mouseID;
  const PadCtx ctx = owner.padContext();

  if(owner.padVideoActive()) { owner.padSkipVideo(); return; }   // any tap skips the intro/cutscene

  if(ctx==PadCtx::World) {
    // A radial ring is open -> corners switch/cancel; every other touch aims
    // and commits on release.
    if(owner.padRingOpen()) {
      const auto c = ringControls();
      if(contains(c[2],pos)) {
        owner.padRingCancel();
        return;
        }
      if(contains(c[0],pos)) {
        owner.padOpenItemRing();
        return;
        }
      if(contains(c[1],pos)) {
        owner.padOpenWeaponsRing();
        return;
        }
      ringId = id;
      aimRing(pos);
      return;
      }

    auto* pl = Gothic::inst().player();
    const WeaponState ws = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
    const bool melee  = ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H;
    const bool ranged = ws==WeaponState::Bow || ws==WeaponState::CBow;
    const bool armed  = ws!=WeaponState::NoWeapon;
    auto holdAction = [&](A action) {
      ctrl.onKeyPressed(action,Event::K_NoKey,M::Primary);
      btnDown[id] = action;
      };

    const auto wl = worldLayout();
    for(auto& b:wl.buttons)
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        switch(b.kind) {
          case TAct::Key:
            holdAction(b.act);
            return;
          case TAct::Interact:
            if(!armed)
              holdAction(A::ActionGeneric);
            return;
          case TAct::Special:
            if(melee)
              holdAction(A::PadSpecial);
            return;
          case TAct::Lt:
            if(!armed)      holdAction(A::WeaponBow);
            else if(melee)  holdAction(A::Parade);
            else if(ranged) holdAction(A::PadAim);
            return;
          case TAct::Rt:
            holdAction(armed ? A::PadAttack : A::WeaponMele);
            return;
          case TAct::Lb:
            if(melee) {
              holdAction(A::PadAttackLeft);
              }
            else if(walkId<0) {
              ctrl.setGamepadWalk(true);
              walkId = id;
              }
            return;
          case TAct::Rb:
            holdAction(melee ? A::PadAttackRight : A::LookBack);
            return;
          case TAct::WeaponsRing:
            releaseWorldTouches(); owner.padOpenWeaponsRing(); return;
          case TAct::ItemRing:
            releaseWorldTouches(); owner.padOpenItemRing(); return;
          case TAct::Lock:       ctrl.toggleTargetLock();   return;
          case TAct::JournalOrFocus:
            if(ctrl.isTargetLocked()) ctrl.focusLeft();
            else owner.uiAction(A::Log);
            return;
          case TAct::MapOrFocus:
            if(ctrl.isTargetLocked()) ctrl.focusRight();
            else owner.padOpenMap();
            return;
          case TAct::SystemView:
            if(viewId<0) {
              viewId = id;
              dispatchSystemEffect(systemGesture.onButton(
                  PadSystemGesture::Button::View,true,Application::tickCount()));
              }
            return;
          case TAct::SystemMenu:
            if(menuId<0) {
              menuId = id;
              dispatchSystemEffect(systemGesture.onButton(
                  PadSystemGesture::Button::Menu,true,Application::tickCount()));
              }
            return;
          }
        }

    if(contains(wl.move,pos)) {
      moveId = id; moveOrigin = pos;
      return;
      }
    if(contains(wl.look,pos)) {
      lookId = id; lookLast = pos;
      return;
      }
    e.ignore();
    return;
    }

  // UI contexts: a tap fires one synthetic key, routed to the active widget.
  auto tap = [&](const auto& arr)->bool{
    for(auto& b:arr)
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        KeyEvent ev(b.key);
        owner.dispatchKey(ev);
        return true;
        }
    return false;
    };

  if(ctx==PadCtx::Dialog) {
    tap(dialogLayout());
    return;
    }
  if(ctx==PadCtx::Inventory) {
    for(auto& b:characterPageLayout())
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        owner.padInventoryCategory(b.direction);
        return;
        }
    }
  if(ctx==PadCtx::Menu && owner.padCharacterPageActive()) {
    for(auto& b:characterPageLayout())
      if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
        owner.padCycleCharacterPage(b.direction);
        return;
        }
    }
#if defined(__IOS__)
  if(ctx==PadCtx::Menu && owner.canOpenDeviceSettings()) {
    const auto b = deviceButton();
    if(pos.x>=b.x && pos.x<b.x+b.s && pos.y>=b.y && pos.y<b.y+b.s) {
      owner.openDeviceSettings();
      return;
      }
    }
#endif
  if(ctx==PadCtx::Menu || ctx==PadCtx::Inventory) {
    tap(menuLayout());
    return;
    }
  e.ignore();
  }

void TouchInput::mouseDragEvent(MouseEvent& e) {
  if(Gamepad::poll().connected) {
    if(owner.padRingOpen())
      return;
    e.ignore();
    return;
    }
  if(owner.padContext()!=PadCtx::World) { e.ignore(); return; }

  const Point pos = e.pos();
  const int   id  = e.mouseID;

  if(id==ringId && owner.padRingOpen()) {
    aimRing(pos);
    return;
    }

  if(id==lookId) {
    const Point d = pos - lookLast;
    lookLast = pos;
    ctrl.onRotateMouse(float(-d.x)*4.f, float(-d.y)*2.f);
    return;
    }

  if(id==moveId) {
    const auto wl = worldLayout();
#if defined(__IOS__)
    const int dz = std::max(1,std::min(wl.move.w,wl.move.h)/5);
#else
    const int dz = h()/16;
#endif
    const int dx = pos.x - moveOrigin.x;
    const int dy = pos.y - moveOrigin.y;
    auto set = [&](int idx, bool on, A a){
      if(on && !mv[idx])      { ctrl.onKeyPressed(a, Event::K_NoKey, M::Primary); mv[idx]=true;  }
      else if(!on && mv[idx]) { ctrl.onKeyReleased(a, M::Primary);                mv[idx]=false; }
      };
    set(0, dy < -dz, A::Forward);
    set(1, dy >  dz, A::Back);
    // pad X turns the character (Gothic-classic rotate), it does not strafe
    set(2, dx < -dz, A::RotateL);
    set(3, dx >  dz, A::RotateR);
    return;
    }
  e.ignore();
  }

void TouchInput::mouseUpEvent(MouseEvent& e) {
  if(Gamepad::poll().connected && owner.padRingOpen())
    return;

  const int id = e.mouseID;

  if(id==ringId) {
    if(owner.padRingOpen())
      owner.padRingCommit();
    ringId = -1;
    return;
    }

  auto releaseSystem = [&](int& touchId, PadSystemGesture::Button button) {
    if(id!=touchId)
      return false;
    touchId = -1;
    dispatchSystemEffect(systemGesture.onButton(
        button,false,Application::tickCount()));
    return true;
    };
  if(releaseSystem(viewId,PadSystemGesture::Button::View) ||
     releaseSystem(menuId,PadSystemGesture::Button::Menu))
    return;

  if(id==walkId) {
    ctrl.setGamepadWalk(false);
    walkId = -1;
    return;
    }

  auto it = btnDown.find(id);
  if(it!=btnDown.end()) {
    ctrl.onKeyReleased(it->second, M::Primary);
    btnDown.erase(it);
    return;
    }

  if(id==moveId) {
    if(mv[0]) ctrl.onKeyReleased(A::Forward, M::Primary);
    if(mv[1]) ctrl.onKeyReleased(A::Back,    M::Primary);
    if(mv[2]) ctrl.onKeyReleased(A::RotateL, M::Primary);
    if(mv[3]) ctrl.onKeyReleased(A::RotateR, M::Primary);
    mv[0]=mv[1]=mv[2]=mv[3]=false;
    moveId = -1;
    return;
    }

  if(id==lookId) { lookId = -1; return; }

  e.ignore();
  }

#endif
