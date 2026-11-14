#include "menu_page.h"
#include "page_id.h"
#include "ui_widgets.h"
#include "managers/page_manager.h"
#include <stdio.h>

namespace pages
{
    MenuPage menu_page;

    namespace
    {
        struct MenuItem
        {
            const char *line1; // first label line (drawn at text size 2)
            const char *line2; // optional second line, nullptr if unused
            PageId target;
        };

        // Add new menu entries here; the carousel and dots adapt automatically.
        constexpr MenuItem ITEMS[] = {
            {"LIVE", "MONITOR", PageId::LIVE_MONITORING},
            {"TEMPLATE", nullptr, PageId::TEMPLATE},
            {"NETWORK", "CONFIG", PageId::NETWORK_CONFIGURATION},
        };
        constexpr int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);

        // Flip to -1 if clockwise turns the carousel the wrong way.
        constexpr int ENCODER_DIRECTION = 1;

        // Layout (128x64)
        constexpr int16_t CARD_X = 14;
        constexpr int16_t CARD_Y = 13;
        constexpr int16_t CARD_W = 100;
        constexpr int16_t CARD_H = 38;
        constexpr int16_t ARROW_CY = CARD_Y + CARD_H / 2;
        constexpr int16_t DOTS_Y = 58;
        constexpr int16_t DOT_SPACING = 10;
        constexpr int16_t LINE_HEIGHT = 16; // text size 2
    } // namespace

    void MenuPage::onEnter(Adafruit_SSD1306 &display)
    {
        draw(display);
    }

    void MenuPage::onExit()
    {
        return;
    }

    void MenuPage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed)
        {
            page_manager::navigateTo(ITEMS[_selectedIndex].target);
            return;
        }

        if (encoder_delta != 0)
        {
            // Wrap around in both directions, even for large/negative deltas.
            long next = (_selectedIndex + encoder_delta * ENCODER_DIRECTION) % ITEM_COUNT;
            if (next < 0)
                next += ITEM_COUNT;

            if (next != _selectedIndex)
            {
                _selectedIndex = (int)next;
                draw(display); // only redraw on change: I2C OLED refresh is slow
            }
        }
    }

    void MenuPage::draw(Adafruit_SSD1306 &display)
    {
        char counter[8];
        snprintf(counter, sizeof(counter), "%d/%d", _selectedIndex + 1, ITEM_COUNT);

        display.clearDisplay();
        widgets::drawHeader(display, "MENU", counter);

        // Card with the selected item
        display.drawRoundRect(CARD_X, CARD_Y, CARD_W, CARD_H, 6, SSD1306_WHITE);

        const MenuItem &item = ITEMS[_selectedIndex];
        const int lines = (item.line2 != nullptr) ? 2 : 1;
        int16_t y = CARD_Y + (CARD_H - lines * LINE_HEIGHT) / 2;

        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        widgets::drawCentered(display, item.line1, CARD_X, CARD_W, y);
        if (item.line2 != nullptr)
            widgets::drawCentered(display, item.line2, CARD_X, CARD_W, y + LINE_HEIGHT);

        // Left / right arrows (carousel wraps, so both are always active)
        display.fillTriangle(3, ARROW_CY, 9, ARROW_CY - 6, 9, ARROW_CY + 6, SSD1306_WHITE);
        display.fillTriangle(124, ARROW_CY, 118, ARROW_CY - 6, 118, ARROW_CY + 6, SSD1306_WHITE);

        // Page-indicator dots
        const int16_t totalWidth = (ITEM_COUNT - 1) * DOT_SPACING;
        const int16_t startX = (display.width() - totalWidth) / 2;
        for (int i = 0; i < ITEM_COUNT; i++)
        {
            const int16_t x = startX + i * DOT_SPACING;
            if (i == _selectedIndex)
                display.fillCircle(x, DOTS_Y, 3, SSD1306_WHITE);
            else
                display.drawCircle(x, DOTS_Y, 2, SSD1306_WHITE);
        }

        display.display();
    }
};