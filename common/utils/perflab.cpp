#include "perflab.h"

#include "gothic.h"

namespace {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
constexpr auto Section = "IOS_PERF_LAB";
#endif
}

bool PerfLab::enabled() {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
  return true;
#else
  return false;
#endif
  }

uint8_t PerfLab::workerParticipants() {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
  const int value = Gothic::settingsGetI(Section,"workerParticipants");
  switch(value) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 6:
      return uint8_t(value);
    default:
      break;
    }
#endif
  return 0; // automatic, identical to the public port
  }

uint8_t PerfLab::skyLutInterval() {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
  const int value = Gothic::settingsGetI(Section,"skyLutInterval");
  if(value==2 || value==4 || value==6)
    return uint8_t(value);
#endif
  return 1;
  }

uint8_t PerfLab::fogLutProfile() {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
  const int value = Gothic::settingsGetI(Section,"fogLutProfile");
  if(value==1 || value==2)
    return uint8_t(value);
#endif
  return 0; // 160x90x64, identical to the public LQ path
  }

uint32_t PerfLab::worldFarPlane() {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
  const int value = Gothic::settingsGetI(Section,"worldFarPlane");
  if(value==60 || value==60000)
    return 60000u;
  if(value==80 || value==80000)
    return 80000u;
#endif
  return 100000u;
  }

uint8_t PerfLab::waterReflectionMode() {
#if defined(OPENGOTHIC_IOS_PERF_LAB)
  const int value = Gothic::settingsGetI(Section,"waterReflectionMode");
  if(value==1 || value==2)
    return uint8_t(value);
#endif
  return 0; // current full-screen masked pass
  }
