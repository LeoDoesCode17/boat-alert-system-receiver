#include "network_status_page.h"
#include "page_id.h"
#include "ui_widgets.h"
#include "managers/page_manager.h"
#include "managers/network_manager.h"

namespace pages
{
    NetworkStatusPage network_status_page;

    namespace
    {
        // Layout (128x64): header y=0..9, three rows, back button y=55..63
        constexpr int16_t ROW_H = 13;
        constexpr int16_t ROW_STEP = 14;
        constexpr int16_t FIRST_ROW_Y = 12;
        constexpr int16_t WIFI_ROW_Y = FIRST_ROW_Y;
        constexpr int16_t MQTT_ROW_Y = FIRST_ROW_Y + ROW_STEP;
        constexpr int16_t IP_ROW_Y = FIRST_ROW_Y + 2 * ROW_STEP;
        constexpr int16_t DOT_X = 9;     // status indicator (filled = connected)
        constexpr int16_t LABEL_X = 18;  // row label
        constexpr int16_t RIGHT_PAD = 6; // space between value text and the row's right edge

        // Border + label on the left + right-aligned value. Returns nothing; dot is optional.
        void drawRow(Adafruit_SSD1306 &display, int16_t y, const char *label, const char *value, bool showDot, bool dotFilled)
        {
            display.drawRoundRect(0, y, display.width(), ROW_H, 3, SSD1306_WHITE);

            if (showDot)
            {
                const int16_t centerY = y + ROW_H / 2;
                if (dotFilled)
                    display.fillCircle(DOT_X, centerY, 3, SSD1306_WHITE);
                else
                    display.drawCircle(DOT_X, centerY, 3, SSD1306_WHITE);
            }

            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);

            const int16_t textY = y + (ROW_H - 8) / 2; // 8px tall glyphs
            display.setCursor(LABEL_X, textY);
            display.print(label);

            int16_t x1, y1;
            uint16_t w, h;
            display.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
            display.setCursor(display.width() - RIGHT_PAD - (int16_t)w, textY);
            display.print(value);
        }
    } // namespace

    void NetworkStatusPage::onEnter(Adafruit_SSD1306 &display)
    {
        draw(display);
    }

    void NetworkStatusPage::onExit()
    {
        return;
    }

    void NetworkStatusPage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed) // the back button is the only (always selected) control
        {
            page_manager::navigateTo(PageId::SETTING);
            return;
        }

        // The network task (core 0) can connect/disconnect at any time,
        // so poll the getters and redraw only when something changed.
        const bool wifi = network_manager::isWifiConnected();
        const bool mqtt = network_manager::isMqttConnected();
        const IPAddress ip = network_manager::getLocalIp();

        if (wifi != _lastWifi || mqtt != _lastMqtt || !(ip == _lastIp))
        {
            draw(display);
        }
    }

    void NetworkStatusPage::draw(Adafruit_SSD1306 &display)
    {
        _lastWifi = network_manager::isWifiConnected();
        _lastMqtt = network_manager::isMqttConnected();
        _lastIp = network_manager::getLocalIp();

        display.clearDisplay();
        widgets::drawHeader(display, "NETWORK STATUS");

        drawRow(display, WIFI_ROW_Y, "Wi-Fi", _lastWifi ? "CONNECTED" : "DISCONNECTED", true, _lastWifi);
        drawRow(display, MQTT_ROW_Y, "MQTT", _lastMqtt ? "CONNECTED" : "DISCONNECTED", true, _lastMqtt);

        // IP only means something while Wi-Fi is up
        const String ipText = _lastWifi ? _lastIp.toString() : String("---");
        drawRow(display, IP_ROW_Y, "IP", ipText.c_str(), false, false);

        widgets::drawBackButton(display);
        display.display();
    }
}