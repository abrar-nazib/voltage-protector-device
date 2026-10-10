#pragma once

#include <Arduino.h>

#define VOLTAGE_CAL 187.0

void voltageBegin();
void voltageAddSample(int rawSample);
float voltageGetRms();
