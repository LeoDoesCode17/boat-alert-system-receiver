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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastUpdate = 0;

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

  if (!network_manager::initialize())
  { // <-- was lora_manager::initialize()
    Serial.println(F("Network init failed — halting."));
    while (true)
    {
      delay(10);
    }
  }
}

void loop()
{
  if (millis() - lastUpdate >= 10)
  {
    lastUpdate = millis();
    network_manager::update(); // <-- was lora_manager::update()
    page_manager::update();
  }
}