#include "qspi_driver.h"

#define MX25_MANUFACTURER_ID 0xC2

#define MX25_CMD_WRSR   0x01
#define MX25_CMD_RDSR   0x05
#define MX25_CMD_WREN   0x06
#define MX25_CMD_SE     0x20
#define MX25_CMD_BE     0xD8
#define MX25_CMD_RDID   0x9F

#define MX25_STATUS_SRWD (1 << 7)
#define MX25_STATUS_QE   (1 << 6)
#define MX25_STATUS_WEL  (1 << 1)
#define MX25_STATUS_WIP  (1 << 0)
#define MX25_PAGE_SIZE   256

#define MX25_QSPI_CSNUM         0
#define MX25_PORT_CS            gpioPortG
#define MX25_PIN_CS             9
#define MX25_PORT_SCLK          gpioPortG
#define MX25_PIN_SCLK           0
#define MX25_PORT_DQ0           gpioPortG
#define MX25_PIN_DQ0            1
#define MX25_PORT_DQ1           gpioPortG
#define MX25_PIN_DQ1            2
#define MX25_PORT_DQ2           gpioPortG
#define MX25_PIN_DQ2            3
#define MX25_PORT_DQ3           gpioPortG
#define MX25_PIN_DQ3            4
#define MX25_PORT_PWR_EN        gpioPortG
#define MX25_PIN_PWR_EN         13
#define MX25_QSPI_LOC           2
#define MX25_QSPI_CSPEN         QSPI_ROUTEPEN_CS0PEN

void QSPI0_IRQHandler(void)
{
  uint32_t flags = QSPI_IntGet(QSPI0);
  QSPI_IntClear(QSPI0, flags);

  GPIO_PinOutClear(gpioPortI, 9);
}

void MX25_WREN(void)
{
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode = MX25_CMD_WREN;
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
}

uint32_t MX25_RDID(void)
{

  uint8_t buffer[3];
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode =  MX25_CMD_RDID;
  stigCmd.readDataSize = 3;
  stigCmd.readBuffer = &buffer;
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
  return (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
}

uint8_t MX25_RDSR(void)
{
  uint8_t status;
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode = MX25_CMD_RDSR;
  stigCmd.readDataSize = 1;
  stigCmd.readBuffer = &status;
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
  return status;
}

static void MX25_WRSR(uint8_t status)
{
  uint8_t bytes[] = {status, 0, 0};
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode = MX25_CMD_WRSR;
  stigCmd.writeDataSize = 1;
  stigCmd.writeBuffer = &bytes;
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
}

void MX25_WriteEnable(void)
{
  MX25_WREN();
  uint32_t status = MX25_RDSR();
  while (!(status & MX25_STATUS_WEL))
  {
    status = MX25_RDSR();
  }
}

void MX25_WriteComplete(void)
{
  uint32_t status = MX25_RDSR();
  while (status & MX25_STATUS_WIP)
  {
    status = MX25_RDSR();
  }
}

void ReadTest(uint32_t * page)
{
  uint32_t i;
  uint32_t word;
  bool ok = true;

  uint16_t* addressTemp = flashAddress;

  for (i = 0; i < 256; i++)
  {
    word = *addressTemp;
    addressTemp++;
    if (word != i)
    {
      ok = false;
    }
  }

  if (ok)
  {
    printf("Read Test Ok\n");
  }
  else
  {
    printf("Read Test Failed!\n");
  }
}
void MX25_SectorErase(uint32_t sector)
{
  MX25_SectorEraseByAddress(sector * 0xFFF);//easier function to use when erasing iteratively
}
void MX25_SectorEraseByAddress(uint32_t address)  //
{
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode = MX25_CMD_SE;
  stigCmd.addrSize = 3;
  stigCmd.address = address;

  MX25_WriteEnable();
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
  MX25_WriteComplete();
}
void MX25_SectorEraseBulk(uint32_t sectorStart, uint32_t sectorEnd)
{
  for (uint32_t i=sectorStart;i<sectorEnd;i++) //each sector is 4k
  {
    MX25_SectorErase(i);
    QSPI_WaitForIdle(QSPI0);
  }
}

void MX25_BlockErase(uint32_t block)
{
  MX25_BlockEraseByAddress(block * 0xFFFF);//easier function to use when erasing iteratively
}
void MX25_BlockEraseByAddress(uint32_t address)  //
{
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode = MX25_CMD_BE;
  stigCmd.addrSize = 3;
  stigCmd.address = address;

  MX25_WriteEnable();
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
  MX25_WriteComplete();
}
void MX25_BlockEraseBulk(uint32_t blockStart, uint32_t blockEnd)
{
  for (uint32_t i=blockStart;i<blockEnd;i++) //each sector is 4k
  {
    MX25_BlockErase(i);
    QSPI_WaitForIdle(QSPI0);
  }
}


void MX25_SetQuadMode(void)
{
  uint8_t status;

  MX25_WREN();
  status = MX25_RDSR();
  while (!(status & MX25_STATUS_WEL))
  {
    status = MX25_RDSR();
  }

  MX25_WRSR(status | MX25_STATUS_QE);

  status = MX25_RDSR();
  while (!(status & MX25_STATUS_QE))
  {
    status = MX25_RDSR();
  }
}

void MX25_Init(void)
{
  uint32_t id = 0;
  // Check ID to make sure device is responding
  while (((id & 0xFF0000) >> 16) != MX25_MANUFACTURER_ID) {
    id = MX25_RDID();
  }

  // Switch to Quad mode on the MX25
  MX25_SetQuadMode();
}

void MX25_Chip_Erase(void)
{
  MX25_WREN();
  QSPI_StigCmd_TypeDef stigCmd = {0};
  stigCmd.cmdOpcode =  0x60;
  QSPI_ExecStigCmd(QSPI0, &stigCmd);
}

uint8_t runTest(void)
{
  uint32_t i;
  uint32_t tempBuffer[64];
  for (i = 0; i < (MX25_PAGE_SIZE / 4); i++)
  {
    tempBuffer[i] = i;
  }
  memcpy(flashAddress,tempBuffer,256);
  printf("Write Pages Finish\n");
  return 1;
}

void WritePage(uint32_t pageNumber, uint16_t dataToBeWritten[128])
{
  uint32_t* addressPointer = (pageNumber*64) + (uint32_t*)flashAddress; //64 32-bit words in a 256bit block
  //memcpy(addressPointer,dataToBeWritten,256);
  /*
  for (uint32_t i = 0;i < 127; i+=2)
  {
    *addressPointer = (dataToBeWritten[i+1] << 16)  | (dataToBeWritten[i]);
    addressPointer++;
  }
  */

  //uint32_t i;
  //uint32_t tempBuffer[64];
  /*
  for (uint32_t i = 0; i < 64; i++)
  {
    addressPointer[i] = dataToBeWritten[i];
  }
  */
  //memcpy(page,tempBuffer,256);
  //printf("Write Pages Finish\n");

}

void setupQspi(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_QSPI0, true);

  QSPI_Init_TypeDef initQspi = QSPI_INIT_DEFAULT;
  initQspi.divisor = 4; // 4X is max BUSCLK speed without using PHY
  QSPI_Init(QSPI0, &initQspi);

  // Select CS1 for MX25
  QSPI0->CONFIG |= MX25_QSPI_CSNUM<<_QSPI_CONFIG_PERIPHCSLINES_SHIFT;

  GPIO_PinModeSet(MX25_PORT_CS,   MX25_PIN_CS,   gpioModePushPull, 0);
  GPIO_PinModeSet(MX25_PORT_SCLK, MX25_PIN_SCLK, gpioModePushPull, 0);
  GPIO_PinModeSet(MX25_PORT_DQ0,  MX25_PIN_DQ0,  gpioModePushPull, 0);
  GPIO_PinModeSet(MX25_PORT_DQ1,  MX25_PIN_DQ1,  gpioModePushPull, 0);
  GPIO_PinModeSet(MX25_PORT_DQ2,  MX25_PIN_DQ2,  gpioModePushPull, 0);
  GPIO_PinModeSet(MX25_PORT_DQ3,  MX25_PIN_DQ3,  gpioModePushPull, 0);

  GPIO_PinModeSet(MX25_PORT_PWR_EN, MX25_PIN_PWR_EN, gpioModePushPull, 1);

  QSPI0->ROUTELOC0 = MX25_QSPI_LOC;
  QSPI0->ROUTEPEN  = (QSPI_ROUTEPEN_SCLKPEN
                      | MX25_QSPI_CSPEN
                      | QSPI_ROUTEPEN_DQ0PEN
                      | QSPI_ROUTEPEN_DQ1PEN
                      | QSPI_ROUTEPEN_DQ2PEN
                      | QSPI_ROUTEPEN_DQ3PEN);

  QSPI_ReadConfig_TypeDef readConfig = QSPI_READCONFIG_DEFAULT;

  readConfig.dummyCycles  = 8;
  readConfig.opCode       = 0x6B;
  readConfig.instTransfer = qspiTransferSingle;
  readConfig.addrTransfer = qspiTransferSingle;
  readConfig.dataTransfer = qspiTransferQuad;

  QSPI_ReadConfig(QSPI0, &readConfig);

  QSPI_WriteConfig_TypeDef writeConfig = QSPI_WRITECONFIG_DEFAULT;

  writeConfig.dummyCycles              = 0;
  writeConfig.opCode                   = 0x38;
  writeConfig.addrTransfer             = qspiTransferQuad;
  writeConfig.dataTransfer             = qspiTransferQuad;
  writeConfig.autoWEL                  = true;

  QSPI_WriteConfig(QSPI0, &writeConfig);

  QSPI_IntEnable (QSPI0, QSPI_IRQMASK_INDIRECTOPDONEMASK); //QSPI_IRQSTATUS_INDIRECTOPDONE
  NVIC_EnableIRQ (QSPI0_IRQn);
  uint32_t flags = QSPI_IntGet(QSPI0);
  QSPI_IntClear(QSPI0, flags);
}

//LDMA_Descriptor_t descLink;
LDMA_TransferCfg_t periTransferTx;

LDMA_Descriptor_t get_DMA_link(uint32_t block_number, uint16_t* src_buffer_pointer)
{
  //uint32_t buffer_value_size = sizeof(src_buffer_pointer[0]) >> 3; //stores if pointer to buffer is 1,2 or 4 bytes
  uint32_t buffer_offset = block_number << 7; //multiply by 128
  uint32_t* newAddress = flashAddress + buffer_offset;
  return (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_M2M_HALF(src_buffer_pointer, newAddress, 4, 1 );
}
void start_DMA(uint32_t block_number, uint16_t* src_buffer_pointer)
{
  LDMA_Descriptor_t descLink = get_DMA_link(block_number, src_buffer_pointer);
  descLink.xfer.doneIfs = true;
  LDMA_StartTransfer(0, (void*)&periTransferTx, (void*)&descLink);
}

void start_DMA_single(uint16_t* flashWriteAddress, uint16_t* src_buffer_pointer)
{
  LDMA_Descriptor_t descLink = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_M2M_HALF(src_buffer_pointer, flashWriteAddress, 1, 1 );
  descLink.xfer.doneIfs = true;
  LDMA_StartTransfer(0, (void*)&periTransferTx, (void*)&descLink);
}


void initLdma(void)
{
  LDMA_Init_t init = LDMA_INIT_DEFAULT;
  LDMA_Init( &init );
  LDMA_TransferCfg_t periTransferTx = LDMA_TRANSFER_CFG_MEMORY();
}


void writeToFlashBlocking(uint32_t blockNumber, uint32_t* data)
{
  writeToFlash(blockNumber, data);
  uint32_t flag = QSPI0->INDIRECTWRITEXFERCTRL;
  while((flag & 0x40) == 0)
  {
    flag = QSPI0->INDIRECTWRITEXFERCTRL;
  }
  GPIO_PinOutClear(gpioPortI, 9);
  //QSPI0->INDIRECTWRITEXFERCTRL |= 0x42;
}
void writeToFlash(uint32_t blockNumber, uint32_t* data)
{
  uint32_t blockAddress = blockNumber * 256;

  QSPI0->INDIRECTWRITEXFERSTART = blockAddress;
  QSPI0->INDIRECTWRITEXFERNUMBYTES = 256;
  QSPI0->INDAHBADDRTRIGGER = (uint32_t)flashAddress;
  QSPI0->INDIRECTTRIGGERADDRRANGE = 8;
  QSPI0->INDIRECTWRITEXFERCTRL |= 0x1;
  //memcpy(flashAddress,data,256);

  uint32_t* testAddress = flashAddress;
  for(uint32_t i=0;i<64;i++)
  {
    uint32_t temp = data[i];
    testAddress[i] = temp;
    testAddress++;
  }

}

bool testFlash(void)
{
  uint8_t returnFlashStatus = true;

    //Erase a sector
  MX25_SectorErase(0);

    //Write
  uint32_t buffer[512];
  for(uint32_t j=0;j<32;j++)
  {
    for(uint32_t i=0;i<64;i++)
    {
      buffer[i] = i+(j * 64);
    }
    writeToFlashBlocking(j,buffer);
  }

    //Read back
  uint32_t* testAddress = flashAddress;
  for(uint32_t i=0 ; i<512 ; i++)
  {
    uint32_t temp = testAddress[i];
    if (temp != i)
    {
      returnFlashStatus = false;
    }
  }
    //Erase Sector again
  MX25_SectorErase(0);

  return returnFlashStatus;
}
