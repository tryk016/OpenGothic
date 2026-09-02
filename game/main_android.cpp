#if defined(__ANDROID__)
#include "gamemain.h"

#include <Tempest/Log>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "system/api/androidapi.h"

#include <exception>
#include <unistd.h>

namespace {
constexpr const char* GameRoot = "/sdcard/OpenGothic";
constexpr const char* GameData = "/sdcard/OpenGothic/Gothic2";
}

extern "C" void android_main(android_app* app) {
  // AndroidApi must know the native_app_glue state before Tempest constructs
  // its first Window and waits for an ANativeWindow.
  Tempest::AndroidApi::setAndroidApp(app);

  if(::access(GameData,R_OK)!=0) {
    Tempest::Log::e("Android game directory is not readable: ",GameData);
    return;
    }

  if(::chdir(GameRoot)!=0)
    Tempest::Log::e("Unable to use Android game directory: ",GameRoot);

  const char* argv[] = {"opengothic","-g",GameData};
  try {
    (void)runOpenGothic(3,argv);
    }
  catch(const std::exception& e) {
    Tempest::Log::e("Fatal Android startup error: ",e.what());
    }
  catch(...) {
    Tempest::Log::e("Fatal unknown Android startup error");
    }
  }
#endif
