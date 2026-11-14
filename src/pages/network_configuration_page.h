#pragma once
#include <Arduino.h> // String
#include <vector>    // std::vector holds the list of lines
#include "i_page.h"

namespace pages
{
    class NetworkConfigurationPage : public IPage
    {
    public:
        void onEnter(Adafruit_SSD1306 &display) override;
        void onExit() override;
        void update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed) override;

    private:
        // What pressing the encoder button does when the cursor is on a line
        enum class Action
        {
            None,      // Info line: pressing does nothing
            Configure, // Start configuration mode (access point + web page)
            Back       // Return to the menu
        };

        // One line of the scrollable content
        struct Line
        {
            String text;
            Action action;
        };

        void buildLines();                               // Builds the full list of lines
        void addLine(const String &text, Action action); // Appends one line to the list
        void addValue(const String &value);              // Appends a value, wrapped over several lines
        void scrollIntoView();                           // Moves the window so the selection is visible
        void render(Adafruit_SSD1306 &display);          // Draws the visible rows + scrollbar

        std::vector<Line> _lines; // The whole page content (can be longer than the screen)
        int _selected = 0;        // Index of the highlighted line
        int _top = 0;             // Index of the line shown on the first visible row
        bool _apError = false;    // true after "Configure" failed to start the access point
    };

    extern NetworkConfigurationPage network_configuration_page;
}