#pragma once
#include "i_page.h"

namespace pages
{
    class ConfigurationModePage : public IPage
    {
    public:
        void onEnter(Adafruit_SSD1306 &display) override;
        void onExit() override;
        void update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed) override;

    private:
        void drawGuide(Adafruit_SSD1306 &display); // "join this AP, open this IP" screen
        void drawSaved(Adafruit_SSD1306 &display); // "credentials saved" screen

        bool _showingSaved = false; // true once we switched to the "saved" screen
    };

    extern ConfigurationModePage configuration_mode_page;
}