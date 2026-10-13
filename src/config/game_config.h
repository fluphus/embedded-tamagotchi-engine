#pragma once
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
#include "../content/sprout/config.h"
#else
#include "../content/artoria/config.h"
#endif
#define SAVE_NVS_NAMESPACE "fate_tama"
#define SAVE_DATA_VERSION 10
#define TIME_SIMULATED 1
#define SIM_MINUTES_PER_REAL_SEC 0
