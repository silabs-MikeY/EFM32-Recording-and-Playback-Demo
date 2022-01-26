#include "delay.h"

volatile uint32_t msTicks = 0;
void SysTick_Handler(void)
{
      /* Increment counter necessary in Delay()*/
      msTicks++;
}
void Delay(uint32_t dlyTicks)
{
      uint32_t curTicks;

      curTicks = msTicks;
      while ((msTicks - curTicks) < dlyTicks) ;
}
void Delay2(uint32_t dlyTicks)
{
      uint32_t finish = DWT->CYCCNT + ((CMU_ClockFreqGet(cmuClock_CORE)/1000)*dlyTicks);

      while ((DWT->CYCCNT) < finish) ;
}
uint32_t get_ms_ticks(void)
{
  return msTicks;
}
