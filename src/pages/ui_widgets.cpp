#include "ui_widgets.h"

namespace pages
{
    namespace widgets
    {
        void drawHeader(Adafruit_SSD1306 &display, const char *title, const char *right)
        {
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);

            display.setCursor(0, 0);
            display.print(title);

            if (right != nullptr)
            {
                int16_t x1, y1;
                uint16_t w, h;
                display.getTextBounds(right, 0, 0, &x1, &y1, &w, &h);
                display.setCursor(display.width() - w, 0);
                display.print(right);
            }

            display.drawFastHLine(0, 9, display.width(), SSD1306_WHITE);
        }

        void drawBackButton(Adafruit_SSD1306 &display)
        {
            display.fillRect(0, BACK_BUTTON_Y, 42, 9, SSD1306_WHITE);
            display.setTextSize(1);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(4, BACK_BUTTON_Y + 1);
            display.print(F("< BACK"));
            display.setTextColor(SSD1306_WHITE);
        }

        void drawCentered(Adafruit_SSD1306 &display, const char *text, int16_t x, int16_t w, int16_t y)
        {
            int16_t x1, y1;
            uint16_t tw, th;
            display.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
            display.setCursor(x + (w - (int16_t)tw) / 2, y);
            display.print(text);
        }
    }
}