#include "current.h"

#include <math.h>

static const int ADC_COUNTS = 1024;
static const float ADC_REFERENCE_MV = 5000.0;

static double baseRms = CURRENT_BASE_DEFAULT;
static double tolerancePct = CURRENT_TOLERANCE_PCT_DEFAULT;
static double rmsUpperLimit;
static unsigned int peakThresholdCounts;

static void deriveLimits()
{
  rmsUpperLimit = baseRms * (1.0 + tolerancePct / 100.0);

  double mvPerCount = ADC_REFERENCE_MV / ADC_COUNTS;
  double offsetCounts = CURRENT_OFFSET_MV / mvPerCount;
  double peakMv = rmsUpperLimit * sqrt(2.0) * CURRENT_SENSITIVITY_MV_PER_A;
  peakThresholdCounts = (unsigned int)(offsetCounts + peakMv / mvPerCount);
}

void currentSetLimits(double newBaseRms, double newTolerancePct)
{
  baseRms = newBaseRms;
  tolerancePct = newTolerancePct;
  deriveLimits();
}

double currentRmsUpperLimit()
{
  return rmsUpperLimit;
}

unsigned int currentPeakThresholdCounts()
{
  return peakThresholdCounts;
}

float calculateCurrentRms(unsigned int rawAdcValue)
{
  double sensorMv = (rawAdcValue / (double)ADC_COUNTS) * ADC_REFERENCE_MV;
  double peakCurrent = (sensorMv - CURRENT_OFFSET_MV) / CURRENT_SENSITIVITY_MV_PER_A;
  return peakCurrent / sqrt(2.0);
}
