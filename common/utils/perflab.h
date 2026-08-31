#pragma once

#include <cstdint>

// Hidden runtime selectors used only by the dedicated iOS performance-lab
// build. Production builds always receive the public-port defaults.
class PerfLab final {
  public:
    enum class NpcPoseMode : uint8_t {
      CullDistantOffscreen = 0,
      FullPose             = 1,
      };

    static bool     enabled();
    static void     refreshRuntimeSelectors();
    static uint8_t  workerParticipants();
    static uint8_t  skyLutInterval();
    static uint8_t  fogLutProfile();
    static uint32_t worldFarPlane();
    static uint8_t  waterReflectionMode();
    static NpcPoseMode npcPoseMode();
  };
