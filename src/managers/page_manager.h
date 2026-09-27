#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config/pin.h"
#include "pages/page_id.h"

namespace page_manager
{
    void initialize(Adafruit_SSD1306 &display);
    void update();
    void navigateTo(pages::PageId id);
} // namespace page_manager
