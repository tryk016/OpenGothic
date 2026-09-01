#if defined(__ANDROID__)
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <Tempest/Application>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Vec>
#include <Tempest/VulkanApi>
#include <Tempest/Window>

#include "system/api/androidapi.h"

using namespace Tempest;

namespace {
class ClearWindow final : public Window {
  public:
    explicit ClearWindow(Device& device)
      :device(device),swapchain(device,hwnd()) {
      }

  private:
    void paintEvent(PaintEvent&) override {
      }

    void resizeEvent(SizeEvent&) override {
      swapchain.reset();
      update();
      }

    void render() override {
      if(swapchain.w()==0 || swapchain.h()==0)
        return;
      try {
        auto cmd = device.commandBuffer();
        {
        auto enc = cmd.startEncoding(device);
        enc.setFramebuffer({{swapchain[swapchain.currentImage()],
                             Vec4(0x3a/255.f,0.f,0.f,1.f),
                             Preserve}});
        }
        Fence sync = device.submit(cmd);
        device.present(swapchain);
        sync.wait();
        }
      catch(const SwapchainSuboptimal&) {
        swapchain.reset();
        }
      }

    Device&   device;
    Swapchain swapchain;
  };
}

extern "C" void android_main(android_app* app) {
  Tempest::AndroidApi::setAndroidApp(app);

  VulkanApi   api;
  Device      device(api);
  Application application;
  ClearWindow window(device);
  application.exec();
  }
#endif
