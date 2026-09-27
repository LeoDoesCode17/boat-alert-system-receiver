#pragma once
#include "i_page.h"

namespace pages
{
    class MenuPage : public IPage
    {
    public:
        void onEnter(Adafruit_SSD1306 &display) override;
        void onExit() override;
        void update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed) override;

    private:
        int _selectedIndex = 0;
    };

    extern MenuPage menu_page;
}