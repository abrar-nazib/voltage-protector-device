#pragma once

#include <Arduino.h>

#define CURRENT_SENSITIVITY_MV_PER_A 185.0
#define CURRENT_OFFSET_MV 2542.0

float calculateCurrentRms(unsigned int rawAdcValue);
