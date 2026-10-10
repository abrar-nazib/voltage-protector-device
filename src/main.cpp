#include <Arduino.h>

#include "current.h"
#include "voltage.h"

static const int VOLTAGE_PIN = A0;
static const int CURRENT_PIN = A1;
static const int SURGE_PIN = 2;
static const unsigned int SURGE_CONFIRM_SAMPLES = 10;
static const unsigned long MEASURE_WINDOW_MS = 200;
static const unsigned long RECOVERY_DELAY_MS = 2000;

static unsigned int currentPeakCounts = 0;
static unsigned int voltageOverCount = 0;
static unsigned int currentOverCount = 0;
static unsigned long windowStart = 0;
static bool tripped = false;
static unsigned long lastSurgeMs = 0;
static bool rmsGateArmed = false;

static unsigned int readChannel(int pin)
{
  analogRead(pin);
  return analogRead(pin);
}

static bool surgeCheck(unsigned int rawCounts, unsigned int thresholdCounts,
                       unsigned int &overCount)
{
  if (rawCounts > thresholdCounts)
  {
    overCount++;
    return overCount >= SURGE_CONFIRM_SAMPLES;
  }
  overCount = 0;
  return false;
}

static void printStatus(float voltage, float current)
{
  Serial.print("V=");
  Serial.print(voltage, 1);
  Serial.print(" I=");
  Serial.print(current, 2);
  Serial.print(" D2=");
  Serial.print(tripped ? "LOW" : "HIGH");
  Serial.print(" vOver=");
  Serial.print(voltageOverCount);
  Serial.print(" iOver=");
  Serial.println(currentOverCount);
}

void setup()
{
  pinMode(SURGE_PIN, OUTPUT);
  digitalWrite(SURGE_PIN, HIGH);

  Serial.begin(9600);
  Serial.println("AC Energy Meter");

  voltageSetLimits(VOLTAGE_BASE_DEFAULT, VOLTAGE_TOLERANCE_PCT_DEFAULT);
  currentSetLimits(CURRENT_BASE_DEFAULT, CURRENT_TOLERANCE_PCT_DEFAULT);

  voltageBegin();
  windowStart = millis();
}

void loop()
{
  unsigned int vRaw = readChannel(VOLTAGE_PIN);
  unsigned int iRaw = readChannel(CURRENT_PIN);

  voltageAddSample(vRaw);
  if (iRaw > currentPeakCounts)
  {
    currentPeakCounts = iRaw;
  }

  bool voltagePeakSurge = surgeCheck(vRaw, voltagePeakThresholdCounts(), voltageOverCount);
  bool currentPeakSurge = surgeCheck(iRaw, currentPeakThresholdCounts(), currentOverCount);
  bool faultNow = voltagePeakSurge || currentPeakSurge;

  if (millis() - windowStart >= MEASURE_WINDOW_MS)
  {
    float voltage = voltageGetRms();
    float current = calculateCurrentRms(currentPeakCounts);

    voltageBegin();
    currentPeakCounts = 0;
    windowStart = millis();

    if (!rmsGateArmed)
    {
      rmsGateArmed = true;
    }
    else if (voltage < voltageRmsLowerLimit() ||
             voltage > voltageRmsUpperLimit() ||
             current > currentRmsUpperLimit())
    {
      faultNow = true;
    }

    printStatus(voltage, current);
  }

  if (faultNow)
  {
    tripped = true;
    lastSurgeMs = millis();
  }
  else if (tripped && millis() - lastSurgeMs >= RECOVERY_DELAY_MS)
  {
    tripped = false;
  }
  digitalWrite(SURGE_PIN, tripped ? LOW : HIGH);
}
