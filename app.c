/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "em_timer.h"
#include "em_prs.h"
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_adc.h"
#include "em_qspi.h"
#include "em_prs.h"
#include "em_ldma.h"
#include "em_vdac.h"
#include "em_emu.h"
#include "qspi_driver.h"
#include "SineWave.h"
#include "delay.h"
#include "application_properties.h"

__attribute__((used)) const ApplicationProperties_t sl_app_properties = {
    /// @brief Magic value indicating that this is an ApplicationProperties_t
    /// Must equal @ref APPLICATION_PROPERTIES_MAGIC
    .magic = APPLICATION_PROPERTIES_MAGIC,

    /// Version number of this struct
    .structVersion = APPLICATION_PROPERTIES_VERSION,

    /// Type of signature this application is signed with
    .signatureType = APPLICATION_SIGNATURE_NONE,

    /// Location of the signature. Typically a pointer to the end of application
    .signatureLocation = 0,

    /// Information about the application
    .app = {

      /// Bitfield representing type of application
      /// e.g. @ref APPLICATION_TYPE_BLUETOOTH_APP
      .type = APPLICATION_TYPE_MCU,

      /// Version number for this application
      .version = 0,

      /// Capabilities of this application
      .capabilities = 0,

      /// Unique ID (e.g. UUID/GUID) for the product this application is built for
      .productId = 0,
    },
  };

#define TIMER_FREQ 22000
#define PRS_CHANNEL 0
#define ADC_FREQ 4000000

#define EXTFLASH_PORT_CS gpioPortG
#define EXTFLASH_PIN_CS 9

#define EXTFLASH_PORT_SCLK gpioPortG
#define EXTFLASH_PIN_SCLK 0

#define EXTFLASH_PORT_DQ0 gpioPortG
#define EXTFLASH_PIN_DQ0 1

#define EXTFLASH_PORT_DQ1 gpioPortG
#define EXTFLASH_PIN_DQ1 2

#define EXTFLASH_PORT_DQ2 gpioPortG
#define EXTFLASH_PIN_DQ2 3

#define EXTFLASH_PORT_DQ3 gpioPortG
#define EXTFLASH_PIN_DQ3 4

#define EXTFLASH_PORT_EN gpioPortG
#define EXTFLASH_PIN_EN 13

#define MX25_CMD_WRSR   0x01
#define MX25_CMD_RDSR   0x05
#define MX25_CMD_WREN   0x06
#define MX25_CMD_SE     0x20
#define MX25_CMD_RDID   0x9F

#define MX25_STATUS_SRWD (1 << 7)
#define MX25_STATUS_QE   (1 << 6)
#define MX25_STATUS_WEL  (1 << 1)
#define MX25_STATUS_WIP  (1 << 0)
#define MX25_PAGE_SIZE   256

#define LDMA_CHANNEL    0

LDMA_TransferCfg_t trans;
LDMA_Descriptor_t descr[2];

uint16_t adcReadingPing[128] __attribute__ ((aligned(4)));
uint16_t adcReadingPong[128] __attribute__ ((aligned(4)));

#define RECORDING 0
#define PLAYBACK 1
uint8_t mode = RECORDING;

uint32_t blocksWritten = 0;

void LDMA_IRQHandler(void)
{
  // Clear interrupt flag
  LDMA_IntClear((1 << LDMA_CHANNEL) << _LDMA_IFC_DONE_SHIFT);
  GPIO_PinOutSet(gpioPortG, 15);

  uint32_t* outputAddress;
  if (LDMA->CH[0].DST == (uint32_t)adcReadingPong)  //check which buffer was most recently
  {
    outputAddress = (uint32_t*)adcReadingPing;
  }
  else
  {
    outputAddress = (uint32_t*)adcReadingPong;
  }
  writeToFlash(blocksWritten++, outputAddress);


//  if (outputAddress > 0)
//  {
//    for(uint8_t i = 0; i < 10 ; i++){}
//  }
//  uint32_t temp = adcReadingPing;
//  VDAC_ChannelOutputSet(VDAC0, 0, temp & 0xFFF);
//  if (temp > 0)
//  {
//    for(uint8_t i = 0; i < 10 ; i++){}
//  }
  GPIO_PinOutClear(gpioPortG, 15);
}

void GPIO_EVEN_IRQHandler(void)
{
  // Clear all even pin interrupt flags
  GPIO_IntClear(0x5555);
  GPIO_PinOutSet(gpioPortH, 14);

  mode = PLAYBACK;
  LDMA_StopTransfer(LDMA_CHANNEL);

  TIMER_IntEnable (TIMER1, TIMER_IEN_OF); //Enable Interrupt on clock overflow condition
  NVIC_EnableIRQ (TIMER1_IRQn);
  uint32_t flags = TIMER_IntGet(TIMER1);
  TIMER_IntClear(TIMER1, flags);
}

void TIMER1_IRQHandler(void)
{
  GPIO_PinOutSet(gpioPortG, 15);

  uint32_t flags = TIMER_IntGet(TIMER1);
  TIMER_IntClear(TIMER1, flags);

  static uint16_t* readAddress = flashAddress;
  if (readAddress < (uint16_t*)(flashAddress + (blocksWritten << 6))) //shift by 6 is a multiply by 64, a word is 4 bytes, a 64 word shift is a 256 byte address shift
  {
    uint16_t sample = *readAddress;
    readAddress++;
    VDAC_ChannelOutputSet(VDAC0, 0, sample & 0xFFF);
  }

  GPIO_PinOutClear(gpioPortG, 15);
}

void initADC(void)
{
  // Declare init structs
  ADC_Init_TypeDef init = ADC_INIT_DEFAULT;
  ADC_InitSingle_TypeDef initSingle = ADC_INITSINGLE_DEFAULT;

  // Enable ADC clock
  CMU_ClockEnable(cmuClock_HFPER, true);
  CMU_ClockEnable(cmuClock_ADC0, true);

  /*
  // Select AUXHFRCO for ADC ASYNC mode so it can run in EM2
  CMU->ADCCTRL = CMU_ADCCTRL_ADC0CLKSEL_AUXHFRCO;
  // Set AUXHFRCO frequency and use it to setup the ADC
  CMU_AUXHFRCOBandSet(cmuAUXHFRCOFreq_50M0Hz);
  CMU_AUXHFRCOFreqSet(cmuAUXHFRCOFreq_50M0Hz);
*/
  // Select AUXHFRCO for ADC ASYNC mode so it can run in EM2
    //CMU->ADCCTRL = CMU_ADCCTRL_ADC0CLKSEL_AUXHFRCO;

  uint32_t prescale = ADC_PrescaleCalc(16000000, 0);
  init.prescale   = prescale;
  uint32_t timebase = ADC_TimebaseCalc(0);
  init.timebase   = timebase;

  // Add external ADC input. See README for corresponding EXP header pin.
  initSingle.posSel = adcPosSelAPORT4XCH11;

  // Basic ADC single configuration
  initSingle.diff = false;              // single-ended
  initSingle.reference  = adcRefVDD;    // 2.5V reference
  initSingle.resolution = adcRes12Bit;  // 12-bit resolution
  initSingle.acqTime    = adcAcqTime128;  // set acquisition time to meet minimum requirements

  // Enable PRS trigger and select channel 0
  initSingle.prsEnable = true;
  initSingle.prsSel = (ADC_PRSSEL_TypeDef) PRS_CHANNEL;

  // Initialize ADC
  ADC_Init(ADC0, &init);
  ADC_InitSingle(ADC0, &initSingle);

  /*
  // Set single data valid level (DVL)
  ADC0->SINGLECTRLX = (ADC0->SINGLECTRLX & ~_ADC_SINGLECTRLX_DVL_MASK) | ((1 << _ADC_SINGLECTRLX_DVL_SHIFT) & _ADC_SINGLECTRLX_DVL_MASK);

  // Clear the Single FIFO
  ADC0->SINGLEFIFOCLEAR = ADC_SINGLEFIFOCLEAR_SINGLEFIFOCLEAR;
*/
}

void initLdmaADC(void)
{
  // Enable CMU clock
  CMU_ClockEnable(cmuClock_LDMA, true);

  // Basic LDMA configuration
  LDMA_Init_t ldmaInit = LDMA_INIT_DEFAULT;

  LDMA_Init(&ldmaInit);

  // Transfers trigger off ADC single conversion complete
  trans = (LDMA_TransferCfg_t)LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_ADC0_SINGLE);

  descr[0] = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_P2M_WORD(&(ADC0->SINGLEDATA), &adcReadingPing, 128, 1);
  descr[1] = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_P2M_WORD(&(ADC0->SINGLEDATA), &adcReadingPong, 128, -1);


  descr[0].xfer.size = ldmaCtrlSizeHalf;
  descr[1].xfer.size = ldmaCtrlSizeHalf;

  descr[0].xfer.blockSize = ldmaCtrlBlockSizeUnit1;    // transfers
  descr[1].xfer.blockSize = ldmaCtrlBlockSizeUnit1;    // transfers

  descr[0].xfer.doneIfs = true;
  descr[1].xfer.doneIfs = true;

  // Initialize transfer
  LDMA_StartTransfer(LDMA_CHANNEL, &trans, descr);

  // Clear pending and enable interrupts for LDMA
  //NVIC_ClearPendingIRQ(LDMA_IRQn);
  //NVIC_EnableIRQ(LDMA_IRQn);
}

void initTimer(void)
{
  CMU_ClockEnable (cmuClock_TIMER1, true);
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;  //initialize clock struct with default values
  timerInit.enable = false;
  TIMER1->TOP = CMU_ClockFreqGet(cmuClock_TIMER1)/TIMER_FREQ;

  CMU_ClockEnable(cmuClock_PRS, true);
  PRS_SourceAsyncSignalSet(PRS_CHANNEL, PRS_CH_CTRL_SOURCESEL_TIMER1, PRS_CH_CTRL_SIGSEL_TIMER1OF);

  TIMER_Init(TIMER1, &timerInit);
  TIMER_Enable (TIMER1, true); //Start TImer
}

void initAudioOutput(void)
{
  // Enable the VDAC clock
  CMU_ClockEnable(cmuClock_VDAC0, true);

  VDAC_Init_TypeDef init = VDAC_INIT_DEFAULT;
  VDAC_InitChannel_TypeDef initChannel = VDAC_INITCHANNEL_DEFAULT;

  init.asyncClockMode = true;  // VDAC clock source for asynchronous mode is 12 MHz internal VDAC oscillator
  init.prescaler = VDAC_PrescaleCalc(1000000, false, 0); // Calculate the VDAC clock prescaler value resulting in a 1 MHz VDAC clock.
  init.reference = vdacRef2V5Ln;  // Set reference to internal 2.5V low noise reference

  // Setup Pin Output
  VDAC0->OPA[0].OUT |= VDAC_OPA_OUT_ALTOUTEN | VDAC_OPA_OUT_ALTOUTPADEN_OUT0;  // Enable alternate output pin C0
  VDAC0->OPA[0].OUT &= ~(VDAC_OPA_OUT_MAINOUTEN); // Disable main output pin
  VDAC0->OPA[0].TIMER &= ~(_VDAC_OPA_TIMER_SETTLETIME_MASK); // Set the settle time to zero for maximum update rate (mask it out)

  // Initialize the VDAC and VDAC channel
  VDAC_Init(VDAC0, &init);
  VDAC_InitChannel(VDAC0, &initChannel, 0);

  // Enable the VDAC
  VDAC_Enable(VDAC0, 0, true);
}


//void beep(uint32_t millis)
//{
//  GPIO_PinOutClear(gpioPortH, 13);
//#define FREQUENCY 50
//  uint32_t sineWaveArraySize = LOOKUP_TABLE_SIZE;
//  uint32_t ticksBetweenSamples = CMU_ClockFreqGet(cmuClock_CORE)/(FREQUENCY * sineWaveArraySize);
//  uint32_t maxValue = sineWaveArraySize * 100 * millis / 1000;
//  uint32_t sineWavePointer = 0;
//  for(uint32_t i = 0; i < maxValue;i++)
//  {
//    uint16_t sample = sine_wave_lookup_table[sineWavePointer] >> 4;
//    sineWavePointer+=2;
//    VDAC_ChannelOutputSet(VDAC0, 0, sample & 0xFFF);
//    if (sineWavePointer > sineWaveArraySize-1)
//    {
//      sineWavePointer=0;
//    }
//    uint32_t currentTime = DWT->CYCCNT;
//    while(DWT->CYCCNT < (currentTime + ticksBetweenSamples)){}
//  }
//  GPIO_PinOutSet(gpioPortH, 13);
//}
//
//void beep2()
//{
//  GPIO_PinOutClear(gpioPortH, 13);
//
//  uint32_t sineWavePointer = 0;
//  for(uint32_t i = 0; i < 1000;i++)
//  {
//    uint16_t sample = sine_wave_lookup_table[sineWavePointer] >> 4;
//    VDAC_ChannelOutputSet(VDAC0, 0, sample & 0xFFF);
//    sineWavePointer += 10;
//    if (sineWavePointer > LOOKUP_TABLE_SIZE-1)
//    {
//      sineWavePointer=0;
//    }
//    Delay(1);
//  }
//
//  GPIO_PinOutSet(gpioPortH, 13);
//}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000);

  GPIO_PinModeSet(gpioPortG,   15,   gpioModePushPull, 0);  //Debug pin out for probing

  GPIO_PinModeSet(gpioPortH, 13, gpioModePushPull, 1); //R active-low
  GPIO_PinModeSet(gpioPortH, 14, gpioModePushPull, 1); //G active-low
  GPIO_PinModeSet(gpioPortH, 15, gpioModePushPull, 1); //B active-low

  GPIO_PinModeSet(gpioPortC, 8, gpioModeInputPullFilter, 1);
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);
  GPIO_ExtIntConfig(gpioPortC, 8,8, 1, 0, true);

  setupQspi();
  if(!testFlash()){while(1){GPIO_PinOutClear(gpioPortH, 13);}}
  MX25_BlockEraseBulk(0,6);
  initAudioOutput();
  GPIO_PinOutClear(gpioPortH, 14);
  //beep2(500);

  initADC();  // Setup ADC to perform conversions via PRS
  initLdmaADC();  // Setup DMA to move ADC results to user memory
  initTimer();  // Set up TIMER1 to trigger ADC via PRS
  //ADC_Start(ADC0, adcStartSingle);
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  //EMU_EnterEM1();
}
