#include "live_monitoring_page.h"
#include "page_id.h"
#include "ui_widgets.h"
#include "managers/page_manager.h"
#include "networks/lora.h"
#include <stdio.h>

namespace pages
{
    LiveMonitoringPage live_monitoring_page;

    namespace
    {
        constexpr int16_t ROW_HEIGHT = 8;
        constexpr int16_t ROW_START = widgets::CONTENT_TOP;
    }

    void LiveMonitoringPage::onEnter(Adafruit_SSD1306 &display)
    {
        draw(display);
    }

    void LiveMonitoringPage::onExit()
    {
        return;
    }

    void LiveMonitoringPage::update(Adafruit_SSD1306 &display, long encoder_delta, bool button_pressed)
    {
        if (button_pressed)
        {
            page_manager::navigateTo(PageId::MENU);
            return;
        }

        // Redraw only when a new packet arrived or the signal state flipped.
        // (lora::update() is still called from the main loop, not from here.)
        const lora::TelemetryPacket &pkt = lora::getLatestPacket();
        const bool lost = lora::isSignalLost();

        if (pkt.lastReceivedMillis != _lastDrawnRx || lost != _lastDrawnLost)
        {
            draw(display);
        }
    }

    void LiveMonitoringPage::draw(Adafruit_SSD1306 &display)
    {
        const lora::TelemetryPacket &pkt = lora::getLatestPacket();
        const bool neverReceived = (pkt.lastReceivedMillis == 0);
        const bool lost = lora::isSignalLost();

        _lastDrawnRx = pkt.lastReceivedMillis;
        _lastDrawnLost = lost;

        char right[16];
        if (neverReceived || lost)
            snprintf(right, sizeof(right), "NO SIGNAL");
        else
            snprintf(right, sizeof(right), "%ddBm", pkt.rssi);

        display.clearDisplay();
        widgets::drawHeader(display, "LIVE", right);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        if (neverReceived)
        {
            widgets::drawCentered(display, "Waiting for data...", 0, display.width(), 28);
        }
        else
        {
            char line[24];
            int16_t y = ROW_START;

            snprintf(line, sizeof(line), "R:%6.1f P:%6.1f", pkt.roll, pkt.pitch);
            display.setCursor(0, y);
            display.print(line);
            y += ROW_HEIGHT;

            snprintf(line, sizeof(line), "Y:%6.1f GPS:%s", pkt.yaw, pkt.gpsFixValid ? "FIX" : "---");
            display.setCursor(0, y);
            display.print(line);
            y += ROW_HEIGHT;

            snprintf(line, sizeof(line), "LAT:%.6f", pkt.latitude);
            display.setCursor(0, y);
            display.print(line);
            y += ROW_HEIGHT;

            snprintf(line, sizeof(line), "LON:%.6f", pkt.longitude);
            display.setCursor(0, y);
            display.print(line);
            y += ROW_HEIGHT;

            snprintf(line, sizeof(line), "MAC:%.17s",
                     pkt.macAddress[0] ? pkt.macAddress : "--");

            display.setCursor(0, y);
            display.print(line);
            y += ROW_HEIGHT;

            // Danger status: inverted bar so it stands out.
            if (pkt.is_danger)
            {
                display.fillRect(0, y - 1, display.width(), ROW_HEIGHT, SSD1306_WHITE);
                display.setTextColor(SSD1306_BLACK);
            }
            display.setCursor(2, y);
            display.print(pkt.is_danger ? "STATUS: DANGER" : "STATUS: SAFE");
            display.setTextColor(SSD1306_WHITE);
        }

        widgets::drawBackButton(display);
        display.display();
    }
}