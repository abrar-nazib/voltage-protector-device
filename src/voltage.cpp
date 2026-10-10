#include "voltage.h"

#include <math.h>

static const int ADC_COUNTS = 1024;
static const float ADC_REFERENCE_MV = 5000.0;

static double dcOffset;
static double sumSquares;
static unsigned int sampleCount;

static double baseRms = VOLTAGE_BASE_DEFAULT;
static double tolerancePct = VOLTAGE_TOLERANCE_PCT_DEFAULT;
static double rmsLowerLimit;
static double rmsUpperLimit;
static unsigned int peakThresholdCounts;

static void deriveLimits()
{
  rmsUpperLimit = baseRms * (1.0 + tolerancePct / 100.0);
  rmsLowerLimit = baseRms * (1.0 - tolerancePct / 100.0);

  double voltsPerCount = VOLTAGE_CAL * ((ADC_REFERENCE_MV / 1000.0) / ADC_COUNTS);
  peakThresholdCounts =
      (unsigned int)(ADC_COUNTS / 2.0 + (rmsUpperLimit * sqrt(2.0)) / voltsPerCount);
}

void voltageSetLimits(double newBaseRms, double newTolerancePct)
{
  baseRms = newBaseRms;
  tolerancePct = newTolerancePct;
  deriveLimits();
}

double voltageRmsLowerLimit()
{
  return rmsLowerLimit;
}

double voltageRmsUpperLimit()
{
  return rmsUpperLimit;
}

unsigned int voltagePeakThresholdCounts()
{
  return peakThresholdCounts;
}

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
