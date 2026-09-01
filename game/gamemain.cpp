#include "gamemain.h"

#include <Tempest/Window>
#include <Tempest/Application>
#include <Tempest/File>
#include <Tempest/Log>

#include <zenkit/Logger.hh>

#include <Tempest/VulkanApi>

#if defined(_MSC_VER)
#include <Tempest/DirectX12Api>
#endif

#if defined(__APPLE__)
#include <Tempest/MetalApi>
#endif

#include "utils/crashlog.h"
#include "mainwindow.h"
#include "gothic.h"
#include "build.h"
#include "commandline.h"

#include <dmusic.h>

#include <cstring>
#include <memory>
#include <string_view>

namespace {

std::string_view selectDevice(const Tempest::AbstractGraphicsApi& api) {
  auto d = api.devices();

  static Tempest::Device::Props p;
  for(auto& i:d)
    // if(i.type==Tempest::DeviceType::Integrated) {
    if(i.type==Tempest::DeviceType::Discrete) {
      p = i;
      return p.name;
      }
  if(d.size()>0) {
    p = d[0];
    return p.name;
    }
  return "";
  }

std::unique_ptr<Tempest::AbstractGraphicsApi> makeApi(const CommandLine& g) {
  Tempest::ApiFlags flg = g.isValidationMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
  switch(g.graphicsApi()) {
    case CommandLine::DirectX12:
#if defined(_MSC_VER)
      return std::make_unique<Tempest::DirectX12Api>(flg);
#else
      break;
#endif
    case CommandLine::Vulkan:
#if !defined(__APPLE__)
      return std::make_unique<Tempest::VulkanApi>(flg);
#else
      break;
#endif
    }

#if defined(__APPLE__)
  return std::make_unique<Tempest::MetalApi>(flg);
#else
  return std::make_unique<Tempest::VulkanApi>(flg);
#endif
  }

void setupLogging() {
  try {
    static Tempest::WFile logFile("log.txt");
    Tempest::Log::setOutputCallback([](Tempest::Log::Mode mode, const char* text) {
      logFile.write(text,std::strlen(text));
      logFile.write("\n",1);
      if(mode==Tempest::Log::Error)
        logFile.flush();
      });
    }
  catch(...) {
    Tempest::Log::e("unable to setup logfile - fallback to console log");
    }
  CrashLog::setup();

  zenkit::Logger::set(zenkit::LogLevel::INFO, [] (zenkit::LogLevel lvl, const char* cat, const char* message) {
    (void)cat;
    switch(lvl) {
      case zenkit::LogLevel::ERROR:
        Tempest::Log::e("[zenkit] ",message);
        break;
      case zenkit::LogLevel::WARNING:
        Tempest::Log::e("[zenkit] ",message);
        break;
      case zenkit::LogLevel::INFO:
        Tempest::Log::i("[zenkit] ",message);
        break;
      case zenkit::LogLevel::DEBUG:
      case zenkit::LogLevel::TRACE:
        Tempest::Log::d("[zenkit] ",message); // unused
        break;
      }
    });
  Dm_setLogger(DmLogLevel_INFO, [](void* ctx, DmLogLevel lvl, char const* msg) {
    (void)ctx;
    switch(lvl) {
      case DmLogLevel_FATAL:
      case DmLogLevel_ERROR:
      case DmLogLevel_WARN:
        Tempest::Log::e("[dmusic] ",msg);
        break;
      case DmLogLevel_INFO:
        Tempest::Log::i("[dmusic] ",msg);
        break;
      case DmLogLevel_DEBUG:
      case DmLogLevel_TRACE:
        Tempest::Log::d("[dmusic] ",msg);
        break;
      }
    },nullptr);
  }

}

int runOpenGothic(int argc,const char** argv) {
  setupLogging();

  Tempest::Log::i(appBuild);
  Workers::setThreadName("Main thread");

  CommandLine          cmd{argc,argv};
  auto                 api     = makeApi(cmd);
  const auto           gpuName = selectDevice(*api);
  CrashLog::setGpu(gpuName);

  Tempest::Device      device{*api,gpuName};
  CrashLog::setGpu(device.properties().name);

  Resources            resources{device};
  Gothic               gothic;
  GameMusic            music;
  gothic.setupGlobalScripts();

  MainWindow           window(device);
  Tempest::Application application;
  return application.exec();
  }
