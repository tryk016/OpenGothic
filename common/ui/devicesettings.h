#pragma once

#include <Tempest/Widget>

#include <array>
#include <cstdint>

class MainWindow;

// Native, platform-owned settings layer.  It deliberately has no connection
// to a Daedalus MENU.DAT instance, so stock and modded game menus stay intact.
class DeviceSettings final : public Tempest::Widget {
  public:
    explicit DeviceSettings(MainWindow& owner);

    bool isOpen() const;
    void open();
    void close();

    void keyDownEvent (Tempest::KeyEvent& event) override;
    void keyUpEvent   (Tempest::KeyEvent& event) override;
    void mouseDownEvent(Tempest::MouseEvent& event) override;
    void mouseDragEvent(Tempest::MouseEvent& event) override;
    void mouseUpEvent  (Tempest::MouseEvent& event) override;

  protected:
    void paintEvent(Tempest::PaintEvent& event) override;

  private:
    enum class Row : uint8_t {
      FrameRate,
      Language,
      };

    struct Box {
      int x=0, y=0, w=0, h=0;
      bool contains(int px, int py) const;
      };

    // One geometry object is shared by painting and touch hit-testing.
    struct Layout {
      Box               panel;
      Box               frameRateRow;
      std::array<Box,3> frameRateButtons;
      Box               languageRow;
      Box               languageButton;
      Box               backButton;
      int               gap=0;
      int               titleBaseline=0;
      int               frameRateBaseline=0;
      int               languageBaseline=0;
      };

    Layout        layout() const;
    int           frameRate() const;
    void          setFrameRate(int value);
    void          cycleFrameRate(int direction);
    void          cycleLanguage(int direction);
    void          cycleActiveRow(int direction);

    MainWindow& owner;
    bool        active = false;
    Row         selectedRow = Row::FrameRate;
  };
