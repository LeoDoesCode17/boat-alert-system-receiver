#include "configuration_mode_page.h"
#include "page_id.h"
#include "ui_widgets.h"
#include "managers/page_manager.h"
#include "managers/network_manager.h"

namespace pages
{
    ConfigurationModePage configuration_mode_page;

    namespace
    {
        constexpr int16_t ROW_Y0 = 14;   // first text row under the header
        constexpr int16_t ROW_STEP = 12; // distance between rows

        void printRow(Adafruit_SSD1306 &display, int row, const String &text)
        {
            display.setCursor(0, ROW_Y0 + row * ROW_STEP);
            display.print(text);
        }
    }

    void ConfigurationModePage::onEnter(Adafruit_SSD1306 &display)
    {
        _showingSaved = false; // Fresh session: show the guide first
        drawGuide(display);
    }

    void ConfigurationModePage::onExit()
    {
        network_manager::stop_ap(); // Leaving this page for ANY reason shuts down AP + web server
    }

    void ConfigurationModePage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        // The web server (served from loop() via network_manager::update_ap) sets this
        // flag when the form is submitted.
        if (!_showingSaved && network_manager::hasSubmittedCredentials())
        {
            _showingSaved = true; // Only switch screens once
            drawSaved(display);
        }

        if (button_pressed) // Cancel (or Back after saving)
        {
            page_manager::navigateTo(PageId::NETWORK_CONFIGURATION); // onExit() above will stop the AP
        }
    }

    void ConfigurationModePage::drawGuide(Adafruit_SSD1306 &display)
    {
        display.clearDisplay();
        widgets::drawHeader(display, "CONFIG MODE");
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        // The guide: join the AP, then open the IP in a browser
        printRow(display, 0, "AP: " + network_manager::getApSsid());
        printRow(display, 1, "Pass: " + network_manager::getApPassword());
        printRow(display, 2, "Open: " + network_manager::getApIp());

        widgets::drawButton(display, "CANCEL"); // The only button, always selected
        display.display();
    }

    void ConfigurationModePage::drawSaved(Adafruit_SSD1306 &display)
    {
        display.clearDisplay();
        widgets::drawHeader(display, "CONFIG MODE");
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        printRow(display, 0, "Credentials saved!");
        printRow(display, 1, "Check Serial Monitor");

        widgets::drawButton(display, "BACK"); // Same button, new label
        display.display();
    }
}