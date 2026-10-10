#include <Arduino.h>

#include "current.h"
#include "voltage.h"

static const int VOLTAGE_PIN = A0;
static const int CURRENT_PIN = A1;
static const int ADC_COUNTS = 1024;
static const float START_WINDOW_LOW = 0.45;
static const float START_WINDOW_HIGH = 0.55;
static const unsigned int VOLTAGE_CROSSINGS = 20;
static const unsigned long VOLTAGE_TIMEOUT_MS = 2000;
static const unsigned int CURRENT_SAMPLES = 500;

static float readVoltageRms()
{
  unsigned long start = millis();
  int startSample;
  while (true) {
    startSample = analogRead(VOLTAGE_PIN);
    if (startSample > ADC_COUNTS * START_WINDOW_LOW &&
        startSample < ADC_COUNTS * START_WINDOW_HIGH) {
      break;
    }
    if (millis() - start > VOLTAGE_TIMEOUT_MS) {
      break;
    }
  }

  voltageBegin();

  start = millis();
  unsigned int crossCount = 0;
  bool lastCross = false;
  bool firstSample = true;

  while (crossCount < VOLTAGE_CROSSINGS &&
         (millis() - start) < VOLTAGE_TIMEOUT_MS) {
    int sample = analogRead(VOLTAGE_PIN);
    voltageAddSample(sample);

    bool checkCross = sample > startSample;
    if (firstSample) {
      lastCross = checkCross;
      firstSample = false;
    } else if (checkCross != lastCross) {
      crossCount++;
    }
    lastCross = checkCross;
  }

  return voltageGetRms();
}

static float readCurrentRms()
{
  unsigned int peakCounts = 0;
  for (unsigned int i = 0; i < CURRENT_SAMPLES; i++) {
    unsigned int sample = analogRead(CURRENT_PIN);
    if (sample > peakCounts) {
      peakCounts = sample;
    }
  }
  return calculateCurrentRms(peakCounts);
}

void setup()
{
  Serial.begin(9600);
}

void loop()
{
  float voltage = readVoltageRms();
  float current = readCurrentRms();

  Serial.print("V=");
  Serial.print(voltage, 2);
  Serial.print(" I=");
  Serial.println(current, 2);
}
