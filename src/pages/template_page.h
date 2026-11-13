#pragma once
#include "i_page.h"

namespace pages
{
    // Placeholder page: copy this as a starting point for new pages.
    class TemplatePage : public IPage
    {
    public:
        void onEnter(Adafruit_SSD1306 &display) override;
        void onExit() override;
        void update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed) override;
    };

    extern TemplatePage template_page;
}