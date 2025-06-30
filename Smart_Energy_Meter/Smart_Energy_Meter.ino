#include "ZMPT101B.h"
#include "ACS712.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define relay 14
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

ZMPT101B voltageSensor(34);
ACS712 currentSensor(ACS712_20A, 36);

float P = 0;
float U = 0;
float I = 0;
long dt = 0;
float CulmPwh = 0;
float units = 0;
long changeScreen = 0;
float lastSample = 0;

unsigned long lasttime = 0;
long ScreenSelect = 0;

// Relay timing variables
bool relayState = false; // true = ON (LOW), false = OFF (HIGH)
unsigned long previousRelayMillis = 0;
const unsigned long relayInterval = 20000;  // 20 seconds ON/OFF toggle

void setup()
{
  Serial.begin(9600);
  delay(100);

  voltageSensor.setSensitivity(0.00045);
  voltageSensor.setZeroPoint(2621);

  currentSensor.setZeroPoint(2943);
  currentSensor.setSensitivity(0.4);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH); // Start with relay OFF

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.display();
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousRelayMillis >= relayInterval) {
    relayState = !relayState;
    previousRelayMillis = currentMillis;
    digitalWrite(relay, relayState ? LOW : HIGH); // LOW = ON
  }

  if (!relayState) {
    U = 0;I = 0;P = 0;
    CulmPwh = 0;
  } else {
    U = voltageSensor.getVoltageAC();I = currentSensor.getCurrentAC();
    // Noise filtering
    if (U < 5 ){U = 0; I = 0; }
    else if (U > 230) U= 230;

    if (I > 0 && I < 0.3) I = 0;  // Ignore low noise current
    if (I > 20.0) I = 0;

    // Only calculate if both are valid
    if (U == 0 || I == 0) {
      P = 0;
      CulmPwh = 0;
    } else {
      P = U * I;
      dt = micros() - lastSample;
      CulmPwh += P * (dt / 3600.0); // in uWh
    }
  }

  units = CulmPwh / 1000; // Convert to mWh

  if (millis() - changeScreen > 5000) {
    ScreenSelect++;
    changeScreen = millis();
  }

  if (millis() - lasttime > 500) {
    switch (ScreenSelect % 4) {
      case 0: displayVoltCurrent(); break;
      case 1: displayInstPower(); break;
      case 2: displayEnergy(); break;
      case 3: displayUnits(); break;
    }
  }

  lastSample = micros();
}

void displayVoltCurrent() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(3);
  displayCenter(String(U, 1) + "V", 3);
  displayCenter(String(I, 2) + "A", 33);
  display.display();
  lasttime = millis();
}

void displayInstPower() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  displayCenter("Power", 3);
  display.setTextSize(3);

  if (P > 1000) {
    displayCenter(String(P / 1000.0, 2) + "kW", 30);
  } else {
    displayCenter(String(P, 1) + "W", 30);
  }

  display.display();
  lasttime = millis();
}

void displayEnergy() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (CulmPwh > 1000000000) {
    display.setTextSize(2);
    displayCenter("Energy kWh", 3);
    display.setTextSize(3);
    displayCenter(String(CulmPwh / 1000000000.0, 2), 30);
  } else if (CulmPwh > 1000000) {
    display.setTextSize(2);
    displayCenter("Energy Wh", 3);
    display.setTextSize(3);
    displayCenter(String(CulmPwh / 1000000.0, 2), 30);
  } else if (CulmPwh > 1000) {
    display.setTextSize(2);
    displayCenter("Energy mWh", 3);
    display.setTextSize(3);
    displayCenter(String(CulmPwh / 1000.0, 2), 30);
  } else {
    display.setTextSize(2);
    displayCenter("Energy uWh", 3);
    display.setTextSize(3);
    displayCenter(String(CulmPwh, 0), 30);
  }

  display.display();
  lasttime = millis();
}

void displayUnits() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (units > 1000000) {
    display.setTextSize(2);
    displayCenter("Units", 3);
    display.setTextSize(3);
    displayCenter(String(units / 1000000.0, 2), 30);
  } else if (units > 1000) {
    display.setTextSize(2);
    displayCenter("MilliUnits", 3);
    display.setTextSize(3);
    displayCenter(String(units / 1000.0, 2), 30);
  } else {
    display.setTextSize(2);
    displayCenter("MicroUnits", 3);
    display.setTextSize(3);
    displayCenter(String(units, 0), 30);
  }

  display.display();
  lasttime = millis();
}

void displayCenter(String text, int line) {
  int16_t x1, y1;
  uint16_t width, height;
  display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  display.setCursor((SCREEN_WIDTH - width) / 2, line);
  display.println(text);
  display.display();
}