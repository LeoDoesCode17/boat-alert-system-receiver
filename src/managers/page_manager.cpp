#include "page_manager.h"
#include "pages/i_page.h"
#include "sensor_manager.h"
#include "pages/landing_page.h"
#include "pages/menu_page.h"

namespace
{
    Adafruit_SSD1306 *display_ptr = nullptr;
    pages::IPage *current_page = nullptr;
    long last_encoder_position = 0;

    pages::IPage *page_for(pages::PageId id)
    {
        switch (id)
        {
        case pages::PageId::LANDING:
            return &pages::landing_page;
            break;

        case pages::PageId::MENU:
            /* code */
            return &pages::menu_page;
            break;

        default:
            return &pages::landing_page;
            break;
        }
        return &pages::landing_page;
    }
}

namespace page_manager
{
    void initialize(Adafruit_SSD1306 &display)
    {
        display_ptr = &display;

        navigateTo(pages::PageId::LANDING);
    }
    void update()
    {
        if (current_page == nullptr || display_ptr == nullptr)
        {
            return;
        }

        const long position = sensor_manager::getEncoderPosition();
        const long delta = position - last_encoder_position;
        last_encoder_position = position;

        const bool button_pressed = sensor_manager::wasEncoderButtonPressed();

        current_page->update(*display_ptr, delta, button_pressed);
    }
    void navigateTo(pages::PageId id)
    {
        if (current_page != nullptr)
        {
            current_page->onExit();
        } 

        sensor_manager::setEncoderPosition(0);
        last_encoder_position = 0;

        current_page = page_for(id);
        current_page->onEnter(*display_ptr);
    }
}