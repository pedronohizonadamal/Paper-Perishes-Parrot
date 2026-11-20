// receiver: reads every frame and logs id + raw bytes
#include <SPI.h>
#define CAN_2515
#include "mcp2515_can.h"

const int SPI_CS_PIN = 9;
mcp2515_can CAN(SPI_CS_PIN);

uint32_t id;
uint8_t  len;
uint8_t  data[8] = { 0 };

void setup() {
  Serial.begin(115200);
  while (CAN_OK != CAN.begin(CAN_500KBPS)) {
    Serial.println("CAN init fail, retry...");
    delay(500);
  }
  Serial.println("CAN init ok (receiver)");
}

void loop() {
  if (CAN_MSGAVAIL != CAN.checkReceive()) return;
  if (CAN_OK != CAN.readMsgBuf(&len, data)) return;
  if (len > 8) len = 8;                       // driver sanity guard
  id = CAN.getCanId();

  char buf[64];                               // 45 needed worst-case; 64 is safe
  int n = sprintf(buf, "RX [%08lX] dlc=%u ", (unsigned long)id, len);
  for (uint8_t i = 0; i < len; ++i) n += sprintf(buf + n, "%02X ", data[i]);
  Serial.println(buf);
}