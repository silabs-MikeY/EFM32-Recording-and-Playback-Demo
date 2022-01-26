#ifndef QSPI_DRIVER_H_
#define QSPI_DRIVER_H_

#include "em_cmu.h"
#include "em_qspi.h"
#include "em_gpio.h"
#include <stdio.h>
#include <string.h>
#include "em_ldma.h"

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

//static const uint32_t*
#define flashAddress ((uint32_t *)0xC0000000)
//#define flashAddress ((uint32_t *)0x04000000)

void qspi_init(void);
void MX25_Init(void);
void MX25_WREN(void);
uint32_t MX25_RDID(void);
void MX25_WriteComplete(void);
void MX25_WriteEnable(void);
uint8_t MX25_RDSR(void);
void MX25_SetQuadMode(void);
void MX25_PageProgram(uint8_t buffer[256], uint32_t inputAddress);
void MX25_Chip_Erase(void);
void MX25_SectorErase(uint32_t sector);
void MX25_SectorEraseBulk(uint32_t sectorStart, uint32_t sectorEnd);
void MX25_SectorEraseByAddress(uint32_t address);
void MX25_BlockErase(uint32_t block);
void MX25_BlockEraseBulk(uint32_t blockStart, uint32_t blockEnd);
void MX25_BlockEraseByAddress(uint32_t address);
void readBlock(uint32_t blockNumber, uint16_t returnData[128]);
//void writeBlock(uint32_t blockNumber, uint16_t dataToBeWritten[128]);
void WritePage(uint32_t pageNumber, uint16_t dataToBeWritten[128]);

uint8_t* readBlockPointer(uint32_t blockNumber);
//void WritePage(uint32_t * page);
void ReadTest(uint32_t * page);
void setupQspi(void);

void setupQspi(void);
void start_DMA(uint32_t block_number, uint16_t* src_buffer_pointer);
LDMA_Descriptor_t get_DMA_link(uint32_t block_number, uint16_t* src_buffer_pointer);
void LDMA_IRQHandler( void );
void start_DMA_single(uint16_t* flashWriteAddress, uint16_t* src_buffer_pointer);
void initLdma(void);
bool testFlash(void);
void writeToFlash(uint32_t blockNumber, uint32_t* data);
void writeToFlashBlocking(uint32_t blockNumber, uint32_t* data);
#endif /* QSPI_DRIVER_H_ */
