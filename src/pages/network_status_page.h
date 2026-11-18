#pragma once
#include <IPAddress.h>
#include "i_page.h"

namespace pages
{
    class NetworkStatusPage : public IPage
    {
    public:
        void onEnter(Adafruit_SSD1306 &display) override;
        void onExit() override;
        void update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed) override;

    private:
        void draw(Adafruit_SSD1306 &display);

        // What is on screen right now, so we only redraw when something changes.
        bool _lastWifi = false;
        bool _lastMqtt = false;
        IPAddress _lastIp; // defaults to 0.0.0.0
    };

    extern NetworkStatusPage network_status_page;
}