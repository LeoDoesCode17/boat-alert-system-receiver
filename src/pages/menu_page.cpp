#include "menu_page.h"
#include "page_id.h"
#include "managers/page_manager.h"

namespace pages
{
    MenuPage menu_page;

    void MenuPage::onEnter(Adafruit_SSD1306 &display)
    {
        _selectedIndex = 0;
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println(F("MENU PAGE"));
        display.display();
    }

    void MenuPage::onExit()
    {
        return;
    }

    void MenuPage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed)
        {
            Serial.println("BUTTON IS PRESSED IN MENU PAGE");
            page_manager::navigateTo(pages::PageId::LANDING);
        }
    }
};