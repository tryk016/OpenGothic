#include "game/padmovement.h"
#include "ui/gamepadstick.h"

#include <cassert>
#include <limits>
#include <iostream>

namespace {
bool near(float a, float b) { return std::abs(a-b)<0.0001f; }
void neutral(GamepadStick stick) { assert(stick.x==0.f && stick.y==0.f); }
}

int main() {
  neutral(gamepadRadialDeadZone(0.06f,0.06f,0.10f));
  neutral(gamepadRadialDeadZone(std::numeric_limits<float>::quiet_NaN(),1.f,0.10f));
  neutral(gamepadRadialDeadZone(1.f,std::numeric_limits<float>::infinity(),0.10f));
  const auto diagonal = gamepadRadialDeadZone(1.f,1.f,0.10f);
  assert(near(std::hypot(diagonal.x,diagonal.y),1.f));
  assert(near(diagonal.x,diagonal.y));
  const auto half = gamepadRadialDeadZone(0.f,0.55f,0.10f);
  assert(near(half.y,0.5f));

  bool active = false;
  neutral(gamepadMovementStick(0.15f,0.f,0.10f,0.18f,active));
  assert(!active);
  assert(gamepadMovementStick(0.2f,0.f,0.10f,0.18f,active).x>0.f);
  assert(active);
  assert(gamepadMovementStick(0.15f,0.f,0.10f,0.18f,active).x>0.f);
  neutral(gamepadMovementStick(0.f,0.f,0.10f,0.18f,active));
  assert(!active);
  neutral(gamepadMovementStick(0.15f,0.f,0.10f,0.18f,active));
  active = true;
  neutral(gamepadMovementStick(0.f,std::numeric_limits<float>::quiet_NaN(),0.10f,0.18f,active));
  assert(!active);

  using namespace PadMovement;
  assert(near(heading(0,0,1),0));
  assert(near(heading(0,1,0),-90));
  assert(near(heading(0,-1,0),90));
  assert(near(std::abs(heading(0,0,-1)),180));
  assert(near(heading(35,1,1),-10));
  assert(near(heading(35,0.1f,0.1f),-10));
  assert(near(turnToward(179,-179,16),181));
  assert(near(turnToward(-179,179,16),-181));
  assert(near(turnToward(0,90,0),0));
  assert(near(turnToward(0,90,1000),18));
  assert(!facingMovement(0,180));
  assert(facingMovement(179,-179));
  assert(facingMovement(0,45));
  assert(!facingMovement(0,45.1f));
  for(int fps : {30,60}) {
    float yaw = 0;
    uint64_t previous = 0;
    for(int frame=1; frame<=fps; ++frame) {
      const uint64_t now = uint64_t(frame)*1000/uint64_t(fps);
      yaw = turnToward(yaw,180,now-previous);
      previous = now;
      }
    assert(near(yaw,180));
    }
  std::cout << "Pad movement/dead-zone regressions passed\n";
  }
