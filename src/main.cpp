#include <Arduino.h>
#include <ctype.h>
#include <string.h>

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

static char cmdBuffer[32];
static unsigned int cmdLength = 0;

static bool parseNumber(const char *text, double minValue, double maxValue,
                        double &value)
{
  if (text[0] == '\0')
  {
    return false;
  }
  value = atof(text);
  return value > minValue && value <= maxValue;
}

static void handleCommand(char *line)
{
  char *space = strchr(line, ' ');
  double value = 0.0;
  bool hasValue = false;

  if (space != NULL)
  {
    *space = '\0';
    value = atof(space + 1);
    hasValue = true;
  }

  for (char *p = line; *p != '\0'; p++)
  {
    *p = toupper(*p);
  }

  if (strcmp(line, "SVB") == 0)
  {
    if (!hasValue || !parseNumber(space + 1, 0.0, 500.0, value))
    {
      Serial.println("ERR BAD_VALUE");
      return;
    }
    voltageSetLimits(value, voltageTolerancePct());
    Serial.print("OK SVB ");
    Serial.println(value, 1);
  }
  else if (strcmp(line, "SVT") == 0)
  {
    if (!hasValue || !parseNumber(space + 1, 0.0, 50.0, value))
    {
      Serial.println("ERR BAD_VALUE");
      return;
    }
    voltageSetLimits(voltageBase(), value);
    Serial.print("OK SVT ");
    Serial.println(value, 1);
  }
  else if (strcmp(line, "SCB") == 0)
  {
    if (!hasValue || !parseNumber(space + 1, 0.0, 100.0, value))
    {
      Serial.println("ERR BAD_VALUE");
      return;
    }
    currentSetLimits(value, currentTolerancePct());
    Serial.print("OK SCB ");
    Serial.println(value, 1);
  }
  else if (strcmp(line, "SCT") == 0)
  {
    if (!hasValue || !parseNumber(space + 1, 0.0, 50.0, value))
    {
      Serial.println("ERR BAD_VALUE");
      return;
    }
    currentSetLimits(currentBase(), value);
    Serial.print("OK SCT ");
    Serial.println(value, 1);
  }
  else if (strcmp(line, "GVB") == 0)
  {
    Serial.print("GVB ");
    Serial.println(voltageBase(), 1);
  }
  else if (strcmp(line, "GVT") == 0)
  {
    Serial.print("GVT ");
    Serial.println(voltageTolerancePct(), 1);
  }
  else if (strcmp(line, "GCB") == 0)
  {
    Serial.print("GCB ");
    Serial.println(currentBase(), 1);
  }
  else if (strcmp(line, "GCT") == 0)
  {
    Serial.print("GCT ");
    Serial.println(currentTolerancePct(), 1);
  }
  else
  {
    Serial.println("ERR UNKNOWN_CMD");
  }
}

static void pollSerial()
{
  while (Serial.available() > 0)
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (cmdLength > 0)
      {
        cmdBuffer[cmdLength] = '\0';
        handleCommand(cmdBuffer);
        cmdLength = 0;
      }
    }
    else if (cmdLength < sizeof(cmdBuffer) - 1)
    {
      cmdBuffer[cmdLength++] = c;
    }
    else
    {
      cmdLength = 0;
      Serial.println("ERR SYNTAX");
      while (Serial.available() > 0 && Serial.read() != '\n')
      {
      }
    }
  }
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
  pollSerial();

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
