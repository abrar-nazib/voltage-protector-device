#pragma once

#include <Arduino.h>

#define CURRENT_SENSITIVITY_MV_PER_A 185.0
#define CURRENT_OFFSET_MV 2542.0
#define CURRENT_BASE_DEFAULT 1.0
#define CURRENT_TOLERANCE_PCT_DEFAULT 10.0

float calculateCurrentRms(unsigned int rawAdcValue);

void currentSetLimits(double baseRms, double tolerancePct);
double currentRmsUpperLimit();
unsigned int currentPeakThresholdCounts();
