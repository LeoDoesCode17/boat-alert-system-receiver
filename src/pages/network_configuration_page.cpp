#include "network_configuration_page.h"
#include "page_id.h"
#include "ui_widgets.h"
#include "managers/page_manager.h"
#include "managers/network_manager.h"

namespace pages
{
    NetworkConfigurationPage network_configuration_page; // The single instance used by PageManager

    namespace
    {
        // Flip to -1 if clockwise scrolls the wrong way.
        constexpr int ENCODER_DIRECTION = 1;

        // Layout (128x64): header y=0..9, then 5 rows of 10px, scrollbar on the right edge.
        constexpr int VISIBLE_ROWS = 5;
        constexpr int16_t ROW_Y0 = 12;                          // top of the first row
        constexpr int16_t ROW_HEIGHT = 10;                      // 9px highlight bar + 1px gap
        constexpr int16_t BAR_WIDTH = 124;                      // highlight bar (leaves room for the scrollbar)
        constexpr int16_t TEXT_X = 2;                           // text starts inside the bar
        constexpr int MAX_CHARS = (BAR_WIDTH - 2 * TEXT_X) / 6; // 20 characters per row at text size 1
        constexpr int VALUE_INDENT = 1;                         // values are indented one space under their label
        constexpr int VALUE_WIDTH = MAX_CHARS - VALUE_INDENT;   // characters left for a value on one row
    }

    void NetworkConfigurationPage::onEnter(Adafruit_SSD1306 &display)
    {
        _apError = false; // Forget any error from a previous visit
        _selected = 0;    // Start at the top so the saved values are visible first
        _top = 0;
        buildLines(); // Read the current config and build the content
        render(display);
    }

    void NetworkConfigurationPage::onExit()
    {
        return;
    }

    void NetworkConfigurationPage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed) // Encoder button was clicked
        {
            switch (_lines[_selected].action) // Decide by the line under the selection
            {
            case Action::Configure:
                if (network_manager::start_ap()) // Try to start AP + web server
                {
                    page_manager::navigateTo(PageId::CONFIGURATION_MODE); // Success: show the guide page
                }
                else
                {
                    _apError = true; // Failure: add an error line on top of the content
                    buildLines();    // Rebuild the list (now including the error line)
                    _selected = 0;   // Jump to the top so the error is visible
                    _top = 0;
                    render(display);
                }
                break;

            case Action::Back:
                page_manager::navigateTo(PageId::MENU); // Return to the menu
                break;

            case Action::None: // Selection is on an info line (SSID, host...): nothing to do
                break;
            }
            return; // Done for this loop iteration
        }

        if (encoder_delta != 0) // Knob was rotated
        {
            const int last = static_cast<int>(_lines.size()) - 1; // Index of the last line
            // Clamped (no wrap-around) because this is a content page, not a carousel.
            const int next = constrain(_selected + static_cast<int>(encoder_delta) * ENCODER_DIRECTION, 0, last);

            if (next != _selected) // Only redraw if the selection really moved
            {
                _selected = next;
                scrollIntoView(); // Move the window if the selection left it
                render(display);
            }
        }
    }

    void NetworkConfigurationPage::buildLines()
    {
        _lines.clear(); // Start from an empty list

        // Read what the web form has delivered (empty strings if nothing was configured yet)
        const WifiConfig wifi = network_manager::getWifiConfig();
        const MqttConfig mqtt = network_manager::getMqttConfig();

        if (_apError)
        {
            addLine("AP start failed!", Action::None); // Shown only after a failed Configure
        }

        addLine("Wi-Fi SSID:", Action::None);    // Label
        addValue(wifi.ssid);                     // Value (wrapped if long)
        addLine("MQTT Host:", Action::None);     // Label
        addValue(mqtt.host);                     // Value (wrapped if long)
        addLine("Configure", Action::Configure); // Button 1
        addLine("Back", Action::Back);           // Button 2
    }

    void NetworkConfigurationPage::addLine(const String &text, Action action)
    {
        _lines.push_back(Line{text, action}); // Append one line to the content
    }

    void NetworkConfigurationPage::addValue(const String &value)
    {
        if (value.length() == 0) // Nothing received yet
        {
            addLine(" Not configured", Action::None);
            return;
        }

        // Cut the value into pieces of VALUE_WIDTH characters, one piece per line
        for (unsigned int pos = 0; pos < value.length(); pos += VALUE_WIDTH)
        {
            addLine(" " + value.substring(pos, pos + VALUE_WIDTH), Action::None); // Leading space = indent
        }
    }

    void NetworkConfigurationPage::scrollIntoView()
    {
        if (_selected < _top) // Selection moved above the window: scroll up
        {
            _top = _selected;
        }
        else if (_selected >= _top + VISIBLE_ROWS) // Selection moved below the window: scroll down
        {
            _top = _selected - (VISIBLE_ROWS - 1);
        }
    }

    void NetworkConfigurationPage::render(Adafruit_SSD1306 &display)
    {
        const int count = static_cast<int>(_lines.size());

        display.clearDisplay();
        widgets::drawHeader(display, "NETWORK CONFIG");
        display.setTextSize(1);

        for (int row = 0; row < VISIBLE_ROWS; row++)
        {
            const int index = _top + row; // Which content line belongs on this row
            if (index >= count)
                break; // Past the end of the content: leave the rest blank

            const int16_t y = ROW_Y0 + row * ROW_HEIGHT;
            const bool selected = (index == _selected);

            if (selected)
            {
                display.fillRect(0, y, BAR_WIDTH, ROW_HEIGHT - 1, SSD1306_WHITE); // inverted highlight bar
            }
            display.setTextColor(selected ? SSD1306_BLACK : SSD1306_WHITE);
            display.setCursor(TEXT_X, y + 1);
            display.print(_lines[index].text);
        }
        display.setTextColor(SSD1306_WHITE);

        // Scrollbar on the right edge, only when there is more content than rows
        if (count > VISIBLE_ROWS)
        {
            const int trackH = VISIBLE_ROWS * ROW_HEIGHT - 1;
            display.drawFastVLine(127, ROW_Y0, trackH, SSD1306_WHITE); // thin track

            int thumbH = trackH * VISIBLE_ROWS / count;
            if (thumbH < 4)
                thumbH = 4;
            const int thumbY = ROW_Y0 + (trackH - thumbH) * _top / (count - VISIBLE_ROWS);
            display.fillRect(125, thumbY, 3, thumbH, SSD1306_WHITE); // thumb
        }

        display.display();
    }
}