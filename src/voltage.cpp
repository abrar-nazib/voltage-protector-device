#include "voltage.h"

#include <math.h>

static const int ADC_COUNTS = 1024;
static const float ADC_REFERENCE_MV = 5000.0;

static double dcOffset;
static double sumSquares;
static unsigned int sampleCount;

void voltageBegin()
{
  dcOffset = ADC_COUNTS / 2.0;
  sumSquares = 0;
  sampleCount = 0;
}

void voltageAddSample(int rawSample)
{
  sampleCount++;
  dcOffset += (rawSample - dcOffset) / ADC_COUNTS;
  double filtered = rawSample - dcOffset;
  sumSquares += filtered * filtered;
}

float voltageGetRms()
{
  double voltsPerCount = VOLTAGE_CAL * ((ADC_REFERENCE_MV / 1000.0) / ADC_COUNTS);
  return voltsPerCount * sqrt(sumSquares / sampleCount);
}
