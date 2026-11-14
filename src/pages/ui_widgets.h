#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Small drawing helpers shared by every page so they all look consistent.
// Screen is 128x64: header occupies y=0..9, bottom button y=55..63.
namespace pages
{
    namespace widgets
    {
        constexpr int16_t CONTENT_TOP = 11;   // first free pixel row under the header
        constexpr int16_t BACK_BUTTON_Y = 55; // top of the bottom-left button

        // Title on the left, optional right-aligned text, divider line below.
        void drawHeader(Adafruit_SSD1306 &display, const char *title, const char *right = nullptr);

        // Highlighted (inverted) button in the bottom-left corner, sized to its label.
        // It is the only focusable element, so the encoder button activates it.
        void drawButton(Adafruit_SSD1306 &display, const char *label);

        // Same as drawButton(display, "< BACK").
        void drawBackButton(Adafruit_SSD1306 &display);

        // Draws text horizontally centered inside [x, x + w) at row y,
        // using the display's current text size.
        void drawCentered(Adafruit_SSD1306 &display, const char *text, int16_t x, int16_t w, int16_t y);
    }
}