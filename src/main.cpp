#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config/pin.h"
#include "managers/page_manager.h"
#include "managers/sensor_manager.h"
#include "managers/network_manager.h"
#include "security/aes.h"

// ---- Display geometry ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1       // No dedicated reset pin on most 0.96" I2C boards
#define SCREEN_ADDRESS 0x3C // Most common address. Some modules use 0x3D.

// ---- Task settings ----
constexpr uint32_t TASK_STACK_BYTES = 8192; // Wi-Fi/MQTT/printf need more than 4096
constexpr UBaseType_t TASK_PRIORITY = 1;

namespace
{
  static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

  void scanI2C()
  {
    Serial.println(F("Scanning I2C bus..."));
    byte found = 0;
    for (byte addr = 1; addr < 127; addr++)
    {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0)
      {
        Serial.printf("  Device found at 0x%02X\n", addr);
        found++;
      }
    }
    if (found == 0)
    {
      Serial.println(F("  No I2C devices found. Check wiring/pull-ups."));
    }
  }
}

// Core 1: LoRa receive, config web server, navigation and UI.
void mainTask(void *pv)
{
  Serial.printf("[RTOS] Core-%d - main task started\n", xPortGetCoreID());
  for (;;)
  {
    network_manager::update_on_core_1(); // LoRa (must only ever run on this core)
    network_manager::update_ap();        // config web server (only does work in AP mode)
    page_manager::update();
    vTaskDelay(pdMS_TO_TICKS(1)); // yield so the idle task / watchdog can run
  }
}

// Core 0: Wi-Fi + MQTT connect/reconnect (publish/receive later).
void networkTask(void *pv)
{
  Serial.printf("[RTOS] Core-%d - network task started\n", xPortGetCoreID());
  network_manager::initialize_on_core_0();
  for (;;)
  {
    network_manager::update_on_core_0(); // was update_on_core_1(): LoRa was being driven from BOTH cores
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300); // let serial settle

  Wire.begin(pin::SDA_PIN, pin::SCL_PIN);
  scanI2C();

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 init failed. Check address/wiring."));
    while (true)
    {
      delay(10);
    }
  }

  if (!aes::initialize())
  {
    Serial.println(F("[AES] Init failed - encrypted TX will not work."));
  }

  sensor_manager::initialize();
  page_manager::initialize(display);
  network_manager::initialize_on_core_1(); // LoRa init + creates the mutexes: must run before the tasks start

  xTaskCreatePinnedToCore(networkTask, "NetworkTask", TASK_STACK_BYTES, NULL, TASK_PRIORITY, NULL, 0);
  xTaskCreatePinnedToCore(mainTask, "MainTask", TASK_STACK_BYTES, NULL, TASK_PRIORITY, NULL, 1);
}

void loop()
{
  vTaskDelete(NULL);
}