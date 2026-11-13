#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Small drawing helpers shared by every page so they all look consistent.
// Screen is 128x64: header occupies y=0..9, back button y=55..63.
namespace pages
{
    namespace widgets
    {
        constexpr int16_t CONTENT_TOP = 11;   // first free pixel row under the header
        constexpr int16_t BACK_BUTTON_Y = 55; // top of the back button

        // Title on the left, optional right-aligned text, divider line below.
        void drawHeader(Adafruit_SSD1306 &display, const char *title, const char *right = nullptr);

        // "< BACK" button in the bottom-left corner, drawn highlighted
        // (it is the only focusable element, so the encoder button activates it).
        void drawBackButton(Adafruit_SSD1306 &display);

        // Draws text horizontally centered inside [x, x + w) at row y,
        // using the display's current text size.
        void drawCentered(Adafruit_SSD1306 &display, const char *text, int16_t x, int16_t w, int16_t y);
    }
}