#pragma once

#include <cstdint>

// Shared reducer for the physical and on-screen controller system buttons.
// Both buttons fire on their initial press. A held button carried from another
// UI context is suppressed until it is released.
class PadSystemGesture final {
  public:
    enum class Button : uint8_t {
      View,
      Menu,
      };

    enum class Effect : uint8_t {
      None,
      Inventory,
      GameMenu,
      };

    constexpr PadSystemGesture() = default;

    constexpr void reset(bool viewHeld=false, bool menuHeld=false) {
      reset(view,viewHeld);
      reset(menu,menuHeld);
      }

    constexpr bool down(Button button) const {
      return state(button).down;
      }

    constexpr Effect onButton(Button button, bool pressed) {
      State& current = state(button);
      if(pressed) {
        if(current.down)
          return Effect::None;
        current.down = true;
        if(current.suppressed)
          return Effect::None;
        return button==Button::View ? Effect::Inventory : Effect::GameMenu;
        }

      current.down = false;
      current.suppressed = false;
      return Effect::None;
      }

  private:
    struct State {
      bool down       = false;
      bool suppressed = false;
      };

    static constexpr void reset(State& value, bool held) {
      value.down       = held;
      value.suppressed = held;
      }

    constexpr State& state(Button button) {
      return button==Button::View ? view : menu;
      }

    constexpr const State& state(Button button) const {
      return button==Button::View ? view : menu;
      }

    State view;
    State menu;
  };

constexpr bool padSystemGestureCompileTests() {
  using B = PadSystemGesture::Button;
  using E = PadSystemGesture::Effect;
  PadSystemGesture g;

  if(g.onButton(B::View,true)!=E::Inventory ||
     g.onButton(B::View,false)!=E::None)
    return false;

  g.reset();
  if(g.onButton(B::View,true)!=E::Inventory ||
     g.onButton(B::View,false)!=E::None)
    return false;

  g.reset();
  if(g.onButton(B::Menu,true)!=E::GameMenu ||
     g.onButton(B::Menu,false)!=E::None)
    return false;

  // A system button carried back from another context cannot immediately
  // reopen a page; it has to return to neutral first.
  g.reset(true,true);
  if(g.onButton(B::View,false)!=E::None ||
     g.onButton(B::Menu,false)!=E::None)
    return false;
  if(g.onButton(B::View,true)!=E::Inventory ||
     g.onButton(B::View,false)!=E::None)
    return false;
  return true;
  }

static_assert(padSystemGestureCompileTests());
