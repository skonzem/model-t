
#include "ch.h"
#include "hal.h"

#include "wspr.h"
#include "datastream.h"
#include "wspr_parser.h"
#include "wspr_http.h"
#include "wspr_tcp.h"
#include "wspr_net.h"
#include "chprintf.h"

#include <stdlib.h>
#include <string.h>


static msg_t
thread_wspr(void* arg);

static void
recv_wspr_pkt(void* arg, wspr_msg_t req, uint8_t* data, uint16_t data_len);

static void
handle_debug(uint8_t* data, uint16_t data_len);

static void
handle_version(uint8_t* data, uint16_t data_len);


static SerialDriver* sd = &SD3;
static wspr_parser_t wspr_parser;
static wspr_msg_handler_t handlers[NUM_WSPR_MSGS] = {
    [WSPR_OUT_VERSION] = handle_version,
    [WSPR_OUT_DEBUG] = handle_debug,
};
static systime_t last_idle;
static Mutex msg_mutex;
static uint8_t msg_chksum;
static datastream_t* msg_data;

//static uint8_t wspr_rx[2][512];
//static VirtualTimer rx_timeout;
//static uint8_t* recv_buf;
//static uint8_t* proc_buf;
//static uint16_t proc_len;
//static const UARTConfig wspr_uart_cfg = {
//    .rxend_cb = wspr_rx_end
//};

void
wspr_init()
{
//  recv_buf = wspr_rx[0];
//  proc_buf = wspr_rx[1];
//  proc_len = 0;
  chMtxInit(&msg_mutex);

  wspr_tcp_init();
  wspr_http_init();
  wspr_net_init();

  sdStart(sd, NULL);
  wspr_parser_init(&wspr_parser, recv_wspr_pkt, NULL);

  chThdCreateFromHeap(NULL, 1024, HIGHPRIO, thread_wspr, NULL);
}

void
wspr_set_handler(wspr_msg_t id, wspr_msg_handler_t handler)
{
  if (id < NUM_WSPR_MSGS)
    handlers[id] = handler;
}

static msg_t
thread_wspr(void* arg)
{
  (void)arg;

  chRegSetThreadName("wspr");

  while (1) {
    uint8_t c = sdGet(sd);
    wspr_parse(&wspr_parser, c);
  }

  return 0;
}

//static void
//wspr_rx_end(UARTDriver* uart)
//{
//  chSysLock();
//
//  if (chVTIsArmedI(&rx_timeout))
//    chVTResetI(&rx_timeout);
//
//  uartStartReceive(uart, sizeof(wspr_rx), wspr_rx);
//  chVTSetI(&rx_timeout, MS2ST(100), wspr_tx_timeout, uart);
//
//  chSysUnlock();
//}
//
//static void
//wspr_rx_timeout(void* arg)
//{
//  UARTDriver* uart = arg;
//  size_t rx_left = uartStopReceive(uart);
//  size_t available = sizeof(wspr_rx) - rx_left;
//
//}

void
wspr_idle()
{
  if ((chTimeNow() - last_idle) > MS2ST(200)) {
    wspr_tcp_idle();
    wspr_net_idle();
    last_idle = chTimeNow();
  }
}

datastream_t*
wspr_msg_start(wspr_msg_t id)
{
  chMtxLock(&msg_mutex);

  sdPut(sd, WSPR_SYNC1_CHAR);
  sdPut(sd, WSPR_SYNC2_CHAR);
  sdPut(sd, id);

  msg_chksum = id;

  msg_data = ds_new(NULL, 1024);

  return msg_data;
}

void
wspr_msg_end()
{
  int i;

  uint16_t data_len = ds_index(msg_data);

  sdPut(sd, data_len);
  sdPut(sd, data_len >> 8);
  if (data_len > 0)
    sdWrite(sd, msg_data->buf, data_len);

  msg_chksum += (data_len & 0xFF);
  msg_chksum += (data_len >> 8) & 0xFF;

  for (i = 0; i < data_len; ++i) {
    msg_chksum += msg_data->buf[i];
  }

  sdPut(sd, msg_chksum);

  ds_free(msg_data);
  msg_data = NULL;

  chMtxUnlock();
}

static void
handle_debug(uint8_t* data, uint16_t data_len)
{
  datastream_t* ds = ds_new(data, data_len);

  char* str = ds_read_str(ds);

  free(str);
  ds_free(ds);
}

static void
handle_version(uint8_t* data, uint16_t data_len)
{
  (void)data;
  (void)data_len;
}

static void
recv_wspr_pkt(void* arg, wspr_msg_t id, uint8_t* data, uint16_t data_len)
{
  (void)arg;

  wspr_msg_handler_t handler = handlers[id];
  if (handler)
    handler(data, data_len);
  else
    chprintf((BaseChannel*)&SD4, "no handler for pkt id %d\r\n", id);
}