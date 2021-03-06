
#include "ch.h"
#include "hal.h"
#include "common.h"
#include "crc/crc32.h"

#include "xflash.h"

#include <stdio.h>
#include <stdlib.h>


#define NO_ADDR 0xFFFFFFFF


// Read Commands
#define CMD_READ      0x03
#define CMD_FAST_READ 0x0B
#define CMD_DOR       0x3B
#define CMD_QOR       0x6B
#define CMD_DIOR      0xBB
#define CMD_QIOR      0xEB
#define CMD_RDID      0x9F
#define CMD_READ_ID   0x90

// Write Control Commands
#define CMD_WREN      0x06
#define CMD_WRDI      0x04

// Erase Commands
#define CMD_P4E       0x20
#define CMD_P8E       0x40
#define CMD_SE        0xD8
#define CMD_BE        0x60

// Program Commands
#define CMD_PP        0x02
#define CMD_QPP       0x32

// Status & Config Register Commands
#define CMD_RDSR      0x05
#define CMD_WRR       0x01
#define CMD_RCR       0x35
#define CMD_CLSR      0x30

// Power Save Commands
#define CMD_DP        0xB9
#define CMD_RES       0xAB

// OTP Commands
#define CMD_OTPP      0x42
#define CMD_OTPR      0x4B


// Status Register Bitmasks
#define SR_WIP   0x01
#define SR_WEL   0x02
#define SR_BP0   0x04
#define SR_BP1   0x08
#define SR_BP2   0x10
#define SR_E_ERR 0x20
#define SR_P_ERR 0x40
#define SR_SRWD  0x80


static void
xflash_txn_begin(void);

static void
xflash_txn_end(void);

static void
send_cmd(uint8_t cmd, uint32_t addr,
    const uint8_t* cmd_tx_buf, uint32_t cmd_tx_len,
    uint8_t* cmd_rx_buf, uint32_t cmd_rx_len);

static uint8_t
read_status_reg(void);

static void
write_enable(void);


static const SPIConfig flash_spi_cfg = {
    .end_cb = NULL,
    .ssport = PORT_SFLASH_CS,
    .sspad = PAD_SFLASH_CS,
    .cr1 = SPI_CR1_CPOL | SPI_CR1_CPHA
};
static Mutex xflash_mutex;


void
xflash_init()
{
  chMtxInit(&xflash_mutex);
}

static void
xflash_txn_begin()
{
#if SPI_USE_MUTUAL_EXCLUSION
  spiAcquireBus(SPI_FLASH);
#endif
  spiStart(SPI_FLASH, &flash_spi_cfg);
  spiSelect(SPI_FLASH);
}

static void
xflash_txn_end()
{
  spiUnselect(SPI_FLASH);
#if SPI_USE_MUTUAL_EXCLUSION
  spiReleaseBus(SPI_FLASH);
#endif
}

static void
send_cmd(uint8_t cmd, uint32_t addr, const uint8_t* cmd_tx_buf, uint32_t cmd_tx_len, uint8_t* cmd_rx_buf, uint32_t cmd_rx_len)
{
  xflash_txn_begin();

  spiSend(SPI_FLASH, 1, &cmd);

  if (addr != NO_ADDR) {
    uint8_t addr_buf[3];
    addr_buf[0] = addr >> 16;
    addr_buf[1] = addr >> 8;
    addr_buf[2] = addr;
    spiSend(SPI_FLASH, 3, addr_buf);
  }

  if (cmd_tx_len > 0)
    spiSend(SPI_FLASH, cmd_tx_len, cmd_tx_buf);

  if (cmd_rx_len > 0)
    spiReceive(SPI_FLASH, cmd_rx_len, cmd_rx_buf);

  xflash_txn_end();
}

static uint8_t
read_status_reg()
{
  uint8_t sr;
  send_cmd(CMD_RDSR, NO_ADDR, NULL, 0, &sr, 1);
  return sr;
}

static void
write_enable()
{
  send_cmd(CMD_WREN, NO_ADDR, NULL, 0, NULL, 0);
}

static int
erase(uint32_t erase_addr)
{
  write_enable();
  send_cmd(CMD_SE, erase_addr, NULL, 0, NULL, 0);

  while (1) {
    uint8_t sr = read_status_reg();
    if (sr & SR_E_ERR) {
      send_cmd(CMD_CLSR, NO_ADDR, NULL, 0, NULL, 0);
      return -1;
    }

    if (sr & SR_WIP)
      chThdSleepMilliseconds(10);
    else
      break;
  }

  return 0;
}

int
xflash_erase(uint32_t addr, uint32_t size)
{
  int bytes_remaining = size;
  uint32_t erase_addr = addr;

  while (bytes_remaining > 0) {
    if ((bytes_remaining < XFLASH_SECTOR_SIZE) ||
        ((erase_addr & (XFLASH_SECTOR_SIZE - 1)) != 0))
      return -1;

    chMtxLock(&xflash_mutex);
    int ret = erase(erase_addr);
    chMtxUnlock();

    if (ret != 0)
      return ret;

    erase_addr += XFLASH_SECTOR_SIZE;
    bytes_remaining -= XFLASH_SECTOR_SIZE;
  }

  return 0;
}

bool
xflash_is_erased(uint32_t addr, uint32_t len)
{
  uint8_t* buf = malloc(256);
  bool erased = true;

  while (len > 0) {
    int read_len = MIN(256, len);
    xflash_read(addr, buf, read_len);

    int i;
    for (i = 0; i < read_len; ++i) {
      if (buf[i] != 0xFF) {
        erased = false;
        break;
      }
    }

    if (!erased)
      break;

    addr += read_len;
    len -= read_len;
  }

  free(buf);

  return erased;
}

static int
page_program(uint32_t addr, const uint8_t* buf, uint32_t buf_len)
{
  write_enable();

  send_cmd(CMD_PP, addr, buf, buf_len, NULL, 0);

  while (1) {
    uint8_t sr = read_status_reg();
    if (sr & SR_P_ERR) {
      send_cmd(CMD_CLSR, NO_ADDR, NULL, 0, NULL, 0);
      return -1;
    }

    if (sr & SR_WIP)
      chThdSleepMilliseconds(10);
    else
      break;
  }

  return 0;
}

int
xflash_write(uint32_t addr, const uint8_t* buf, uint32_t buf_len)
{
  uint32_t data_to_write = (XFLASH_PAGE_SIZE - (addr % XFLASH_PAGE_SIZE));
  data_to_write = MIN(data_to_write, buf_len);

  while (buf_len != 0) {
    chMtxLock(&xflash_mutex);
    int ret = page_program(addr, buf, data_to_write);
    chMtxUnlock();

    if (ret != 0)
      return ret;

    addr += data_to_write;
    buf += data_to_write;
    buf_len -= data_to_write;
    data_to_write = MIN(XFLASH_PAGE_SIZE, buf_len);
  }

  return 0;
}

void
xflash_read(uint32_t addr, uint8_t* buf, uint32_t buf_len)
{
  chMtxLock(&xflash_mutex);
  send_cmd(CMD_READ, addr, NULL, 0, buf, buf_len);
  chMtxUnlock();
}

uint32_t
xflash_crc(uint32_t addr, uint32_t size)
{
  uint8_t* buf = calloc(1, 256);
  uint32_t crc = 0xFFFFFFFF;

  while (size > 0) {
    uint32_t nrecv = MIN(size, 256);

    xflash_read(addr, buf, nrecv);

    crc = crc32_block(crc, buf, nrecv);

    addr += nrecv;
    size -= nrecv;
  }

  free(buf);

  return crc;
}
