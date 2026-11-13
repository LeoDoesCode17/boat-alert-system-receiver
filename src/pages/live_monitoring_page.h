#pragma once
#include "i_page.h"

namespace pages
{
    class LiveMonitoringPage : public IPage
    {
    public:
        void onEnter(Adafruit_SSD1306 &display) override;
        void onExit() override;
        void update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed) override;

    private:
        void draw(Adafruit_SSD1306 &display);

        // What was on screen at the last draw, so we only redraw on change.
        unsigned long _lastDrawnRx = 0;
        bool _lastDrawnLost = true;
    };

    extern LiveMonitoringPage live_monitoring_page;
}