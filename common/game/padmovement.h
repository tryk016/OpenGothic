#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace PadMovement {
inline float heading(float cameraYaw, float x, float y) {
  return cameraYaw - std::atan2(x,y)*float(180.0/3.141592653589793);
  }

inline float turnToward(float current, float target, uint64_t dt) {
  const float delta = std::remainder(target-current,360.f);
  const float step  = 360.f*float(std::min<uint64_t>(dt,50))/1000.f;
  return current + std::clamp(delta,-step,step);
  }

inline bool facingMovement(float current, float target) {
  return std::abs(std::remainder(target-current,360.f))<=45.f;
  }
}
