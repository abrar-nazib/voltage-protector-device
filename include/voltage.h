#pragma once

#include <Arduino.h>

#define VOLTAGE_CAL 187.0
#define VOLTAGE_BASE_DEFAULT 230.0
#define VOLTAGE_TOLERANCE_PCT_DEFAULT 5.0

void voltageBegin();
void voltageAddSample(int rawSample);
float voltageGetRms();

void voltageSetLimits(double baseRms, double tolerancePct);
double voltageBase();
double voltageTolerancePct();
double voltageRmsLowerLimit();
double voltageRmsUpperLimit();
unsigned int voltagePeakThresholdCounts();
