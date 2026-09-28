#include <TargetConditionals.h>
#if !TARGET_OS_SIMULATOR
#error This in-process input probe is for Simulator only.
#endif

#include "mainwindow.h"
#include "gothic.h"
#include "world/objects/npc.h"

#include <cstdio>

// Loaded only by LLDB into a paused QA game, never linked into the application.
// Compile against that app's flags with -fno-access-control and -bundle_loader.
extern "C" int testControllerAttack(GamepadInput* pad) {
  using A = KeyCodec::Action;
  using B = GamepadButton;
  auto& touch = pad->owner.mobileUi;
  auto& ctrl = pad->ctrl;
  auto* pl = Gothic::inst().player();
  if(pl==nullptr || pad->owner.padContext()!=PadCtx::World || Gamepad::poll().connected)
    return -1;

  struct Restore {
    GamepadInput& pad;
    TouchInput& touch;
    Npc& player;
    WeaponState weapon;
    ~Restore() {
      touch.releaseWorldTouches();
      pad.releaseAllWorld();
      pad.ctrl.clearInput();
      player.visual.fgtMode = weapon;
      }
    } restore{*pad,touch,*pl,pl->weaponState()};

  unsigned checks = 0;
#define CHECK(condition) do { \
  ++checks; \
  if(!(condition)) { \
    std::printf("FAIL controller attack line %d: %s\n",__LINE__,#condition); \
    return __LINE__; \
    } \
  } while(false)

  GamepadState state;
  auto frame = [&](std::initializer_list<GamepadButtonEvent> events = {}) {
    pad->tickWorld(16,state,std::vector<GamepadButtonEvent>(events));
    pad->prev = state;
    };
  auto reset = [&](WeaponState weapon) {
    touch.releaseWorldTouches();
    pad->releaseAllWorld();
    ctrl.clearInput();
    pl->visual.fgtMode = weapon;
    state = {};
    state.connected = true;
    pad->prev = state;
    frame();
    touch.tick();
    };
  auto attack = [&] { return ctrl.ctrl[A::PadAttack]; };
  auto pendingAttack = [&] { return ctrl.actrl[PlayerControl::ActForward]; };
  auto consumeAttack = [&] { ctrl.actrl[PlayerControl::ActForward] = false; };
  auto down = [&](PadGlyph::Btn glyph, int id) {
    for(const auto& b:touch.worldLayout().buttons) {
      if(b.glyph!=glyph)
        continue;
      Tempest::MouseEvent event(b.x+b.s/2,b.y+b.s/2,Tempest::Event::ButtonLeft,
                               Tempest::Event::M_NoModifier,0,id,Tempest::Event::MouseDown);
      touch.mouseDownEvent(event);
      return;
      }
    };
  auto up = [&](int id) {
    Tempest::MouseEvent event(0,0,Tempest::Event::ButtonLeft,
                             Tempest::Event::M_NoModifier,0,id,Tempest::Event::MouseUp);
    touch.mouseUpEvent(event);
    };

  // Fixtures select weapon modes without ticking the world or changing saves.
  // These assert routing/ownership, not ammunition, damage or spell execution.
  for(auto weapon:{WeaponState::Fist,WeaponState::W1H,WeaponState::W2H,
                   WeaponState::Bow,WeaponState::CBow,WeaponState::Mage}) {
    reset(weapon);
    state.a = true;
    frame({{B::A,true}});
    CHECK(attack() && pendingAttack());
    consumeAttack();
    state.rt = 1.f;
    frame();
    CHECK(attack() && !pendingAttack());
    state.a = false;
    frame({{B::A,false}});
    CHECK(attack() && !pendingAttack());
    state.rt = 0.f;
    frame();
    CHECK(!attack());

    reset(weapon);
    state.rt = 1.f;
    frame();
    CHECK(attack() && pendingAttack());
    consumeAttack();
    state.a = true;
    frame({{B::A,true}});
    CHECK(attack() && !pendingAttack());
    state.rt = 0.f;
    frame();
    CHECK(attack() && !pendingAttack());
    state.a = false;
    frame({{B::A,false}});
    CHECK(!attack());

    reset(weapon);
    frame({{B::A,true},{B::A,false}});
    CHECK(attack() && pendingAttack());
    frame();
    CHECK(!attack());

    reset(weapon);
    frame({{B::A,true},{B::A,false}});
    consumeAttack();
    state.rt = 1.f;
    frame();
    CHECK(attack() && !pendingAttack());
    state.rt = 0.f;
    frame();
    CHECK(!attack());

    reset(weapon);
    state.rt = 1.f;
    frame();
    consumeAttack();
    state.rt = 0.f;
    frame({{B::A,true},{B::A,false}});
    CHECK(attack() && pendingAttack());
    frame();
    CHECK(!attack());

    for(bool aFirst:{true,false}) {
      reset(weapon);
      down(aFirst ? PadGlyph::A : PadGlyph::RT,1);
      CHECK(attack() && pendingAttack());
      consumeAttack();
      down(aFirst ? PadGlyph::RT : PadGlyph::A,2);
      CHECK(attack() && !pendingAttack());
      up(1);
      CHECK(attack() && !pendingAttack());
      up(2);
      CHECK(!attack() && touch.btnDown.empty());
      }
    std::printf("PASS controller/touch aliases weapon=%d\n",int(weapon));
    }

  reset(WeaponState::Bow);
  state.lt = 1.f;
  frame();
  CHECK(ctrl.ctrl[A::PadAim]);
  state.a = true;
  frame({{B::A,true}});
  state.rt = 1.f;
  frame();
  state.a = false;
  frame({{B::A,false}});
  CHECK(attack() && ctrl.ctrl[A::PadAim]);
  state.rt = 0.f;
  frame();
  CHECK(!attack() && ctrl.actrl[PlayerControl::ActGeneric]);

  reset(WeaponState::NoWeapon);
  state.a = true;
  frame({{B::A,true}});
  CHECK(ctrl.ctrl[A::ActionGeneric] && !attack());
  pl->visual.fgtMode = WeaponState::W1H;
  frame();
  CHECK(!attack() && !ctrl.ctrl[A::ActionGeneric] && pad->suppressAUntilRelease);
  state.a = false;
  frame({{B::A,false}});
  state.a = true;
  frame({{B::A,true}});
  CHECK(attack());
  pl->visual.fgtMode = WeaponState::NoWeapon;
  state.rt = 1.f; // Fresh RT must not be suppressed by A's previous attack.
  frame();
  CHECK(!attack() && !ctrl.ctrl[A::ActionGeneric] && !pad->suppressRtUntilRelease);
  CHECK(pad->worldHeld[A::WeaponMele]);

  reset(WeaponState::W1H);
  state.a = true;
  state.rt = 1.f;
  frame({{B::A,true}});
  pad->releaseAllWorld(); // Same reset used for menu/rings/disconnect.
  CHECK(!attack());
  frame();
  CHECK(!attack());
  state.a = false;
  frame({{B::A,false}});
  state.a = true;
  frame({{B::A,true}});
  CHECK(attack()); // A rearms independently of the still-carried RT.

  reset(WeaponState::NoWeapon);
  down(PadGlyph::A,1);
  CHECK(ctrl.ctrl[A::ActionGeneric] && !attack());
  pl->visual.fgtMode = WeaponState::W1H;
  touch.tick();
  CHECK(!ctrl.ctrl[A::ActionGeneric] && !attack());
  up(1);
  down(PadGlyph::A,2);
  CHECK(attack());
  down(PadGlyph::RT,3);
  pl->visual.fgtMode = WeaponState::NoWeapon;
  touch.tick();
  CHECK(!attack() && !ctrl.ctrl[A::ActionGeneric]);
  up(2);
  up(3);
  CHECK(touch.btnDown.empty());
  down(PadGlyph::A,4);
  CHECK(ctrl.ctrl[A::ActionGeneric]);
  up(4);
  CHECK(!ctrl.ctrl[A::ActionGeneric]);

  reset(WeaponState::Mage);
  down(PadGlyph::A,1);
  down(PadGlyph::RT,2);
  touch.releaseWorldTouches();
  CHECK(!attack() && touch.btnDown.empty());

  std::printf("PASS contextual A/RT probe: %u checks\n",checks);
  return 0;
#undef CHECK
  }
