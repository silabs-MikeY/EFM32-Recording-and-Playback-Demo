#ifndef DELAY
#define DELAY

#include "em_cmu.h"

void Delay(uint32_t dlyTicks);
uint32_t get_ms_ticks(void);
void Delay2(uint32_t dlyTicks);
#endif
