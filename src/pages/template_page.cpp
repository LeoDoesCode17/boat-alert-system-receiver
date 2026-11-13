#include "template_page.h"
#include "page_id.h"
#include "ui_widgets.h"
#include "managers/page_manager.h"

namespace pages
{
    TemplatePage template_page;

    void TemplatePage::onEnter(Adafruit_SSD1306 &display)
    {
        display.clearDisplay();
        widgets::drawHeader(display, "TEMPLATE");

        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        widgets::drawCentered(display, "Template page", 0, display.width(), 24);
        widgets::drawCentered(display, "(nothing here yet)", 0, display.width(), 34);

        widgets::drawBackButton(display);
        display.display();
    }

    void TemplatePage::onExit()
    {
        return;
    }

    void TemplatePage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed)
        {
            page_manager::navigateTo(PageId::MENU);
        }
    }
}