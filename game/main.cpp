#include "gamemain.h"

#if defined(__IOS__)
#include "utils/installdetect.h"

#include <filesystem>
#endif

int main(int argc,const char** argv) {
#if defined(__IOS__)
  auto appdir = InstallDetect::applicationSupportDirectory();
  std::filesystem::current_path(appdir);
#endif

  return runOpenGothic(argc,argv);
  }
