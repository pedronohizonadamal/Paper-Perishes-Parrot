// sender: periodically transmits distances from a list, lab frame format
#include <SPI.h>
#define CAN_2515              // change to CAN_2518FD only if THIS shield is a 2518FD
#include "mcp2515_can.h"

const int SPI_CS_PIN = 9;
mcp2515_can CAN(SPI_CS_PIN);

#define DELAY 2000

const uint32_t CAN_ID   = 0x120;

const uint16_t DATA_VALUES[] = { 1, 2, 3, 4};
const uint8_t  N = sizeof(DATA_VALUES) / sizeof(DATA_VALUES[0]);

uint16_t seq = 0, index = 0;
unsigned long lastSent = 0;

SPISettings CAN_SPI(10000000, MSBFIRST, SPI_MODE0);  // 10 MHz, mode 0

uint8_t regRead(uint8_t addr) {
    SPI.beginTransaction(CAN_SPI);
    digitalWrite(SPI_CS_PIN, LOW);        // select the MCP2515
    SPI.transfer(0x03);                   // READ instruction
    SPI.transfer(addr);
    uint8_t value = SPI.transfer(0x00);   // clock out dummy byte, capture reply
    digitalWrite(SPI_CS_PIN, HIGH);
    SPI.endTransaction();
    return value;
}

// returns true if the frame was transmitted AND acknowledged within timeout
bool waitForAck(uint32_t timeout_ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeout_ms) {
    uint8_t ctrl = regRead(0x30);          // TXB0CTRL
    if (!(ctrl & 0x08)) return true;       // TXREQ cleared = frame completed (ACKed)
    if (ctrl & 0x70) {                     // TXERR / TXABT / MLOA bits set
      Serial.print("TX fault, TXB0CTRL=0x"); Serial.println(ctrl, HEX);
      return false;
    }
  }
  return false;                            // still retrying at timeout => no ACK
}

void setup() {
  Serial.begin(115200);
  // NOTE: no "while (!Serial);" here — this is what froze the sketch
  // whenever the PC was unplugged.
  while (CAN_OK != CAN.begin(CAN_500KBPS)) {
    Serial.println("CAN init fail, retry...");
    delay(500);
  }
  Serial.println("CAN init ok (sender)");
}

void next_message(uint8_t *data) {
    const uint16_t data_value = DATA_VALUES[index];
    data[0] = 0;
    data[1] = CAN_ID;
    data[2] = 0;
    data[3] = 16;
    data[4] = data_value & 0xFF;
    data[5] = 0; //ToDo (CRC)
    data[6] = 1;
    data[7] = 0xFF >> 1;
}

void printer(uint16_t seq, uint8_t *data){
    Serial.print((int) seq);
    Serial.print(": ");
    for (int i = 0; i < 8; i++) {
        Serial.print((int) data[i]);
        Serial.print(" ");
    }
    Serial.print("\n");
}

void loop() {
    unsigned long now = millis();
    if (now - lastSent < DELAY) return;
    lastSent = now;

    uint8_t expected_return[8] = {0}, data[8] = { 0 },
            read_data[8] = {0};
    uint32_t id;
    uint8_t len;

    next_message(data);

    if (CAN.sendMsgBuf(CAN_ID, 0, 8, data) == CAN_OK) {
    if (waitForAck(50)) {                    // 50 ms is generous for one frame
        Serial.println("TX: sent + HW ACK");
        printer(seq, data);
    } else {
        Serial.println("TX: no ACK (receiver offline?)");
    }
    } else {
        Serial.println("SEND FAILED");
    }

    uint8_t eflg = CAN.checkError();
    if (eflg) {
        Serial.print("EFLG=0x"); Serial.print(eflg, HEX);
        Serial.print(" TEC=");   Serial.print(regRead(0x1C));
        Serial.print(" REC=");   Serial.println(regRead(0x1D));
    }

    delay(DELAY);
    seq++;
    index = (index + 1) % N;
}