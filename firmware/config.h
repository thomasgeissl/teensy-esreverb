#pragma once

#define PIN_MIX_POTI A0

#define SAMPLES_PER_SECOND 44100
#define MIN_BUFFER_TIME 0.1f    // 0.1 seconds minimum
#define MAX_BUFFER_TIME 2.0f    // 2 seconds maximum
#define HYSTERESIS_TIME 0.1f    // 0.1 seconds of hysteresis

#define MIN_BUFFER_SIZE (SAMPLES_PER_SECOND * MIN_BUFFER_TIME)
#define MAX_BUFFER_SIZE (SAMPLES_PER_SECOND * MAX_BUFFER_TIME)
#define BUFFER_HYSTERESIS (SAMPLES_PER_SECOND * HYSTERESIS_TIME)
#define UPDATE_INTERVAL 1000    // Check for size updates every 1000ms

