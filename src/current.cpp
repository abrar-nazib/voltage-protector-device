#include "current.h"

#include <math.h>

static const int ADC_COUNTS = 1024;
static const float ADC_REFERENCE_MV = 5000.0;

float calculateCurrentRms(unsigned int rawAdcValue)
{
  double sensorMv = (rawAdcValue / (double)ADC_COUNTS) * ADC_REFERENCE_MV;
  double peakCurrent = (sensorMv - CURRENT_OFFSET_MV) / CURRENT_SENSITIVITY_MV_PER_A;
  return peakCurrent / sqrt(2.0);
}
