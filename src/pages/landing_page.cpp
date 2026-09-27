#include "landing_page.h"
#include "page_id.h"
#include "managers/page_manager.h"

#define SCREEN_WIDTH 128

namespace pages
{
    LandingPage landing_page;

    void LandingPage::onEnter(Adafruit_SSD1306 &display)
    {
        _selectedIndex = 0;
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);

        int16_t x1, y1;
        uint16_t w, h;

        display.setTextSize(2);
        display.getTextBounds("BOAT", 0, 4, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 4);
        display.println("BOAT");

        display.setTextSize(2);
        display.getTextBounds("ALERT", 0, 24, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 24);
        display.println("ALERT");

        display.setTextSize(2);
        display.getTextBounds("SYSTEM", 0, 44, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 44);
        display.println("SYSTEM");
        display.display();
    }

    void LandingPage::onExit()
    {
        return;
    }

    void LandingPage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed)
        {
            Serial.println("BUTTON IS PRESSED IN LANDING PAGE");
            page_manager::navigateTo(pages::PageId::MENU);
        }
    }
}