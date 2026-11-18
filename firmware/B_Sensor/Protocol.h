#ifndef CANLAB_PROTOCOL_H
#define CANLAB_PROTOCOL_H
#include <stdint.h>

namespace Lab {
const uint16_t SENSOR_ID = 0x120;  // owner B
const uint16_t MOTOR_ID = 0x220;   // owner A
const uint8_t VERSION = 1;
const uint8_t DLC = 8;
const uint16_t STOP_CM = 50;
const uint16_t MAX_CM = 5000;
const uint32_t SAMPLE_MS = 250;
const uint32_t STALE_MS = 1250;
const uint32_t TX_DEADLINE_MS = 100;
enum MotorState : uint8_t { STOP = 0, RUN = 1, SENSOR_FAULT = 2 };
enum Order : uint8_t { FIRST, FORWARD, DUPLICATE, OLD };

inline uint16_t u16(const uint8_t *p) {
  return (uint16_t(p[0]) << 8) | p[1];
}
inline void put16(uint8_t *p, uint16_t v) {
  p[0] = uint8_t(v >> 8); p[1] = uint8_t(v);
}
inline bool elapsed(uint32_t now, uint32_t then, uint32_t interval) {
  return uint32_t(now - then) >= interval;
}
inline Order order(bool have, uint16_t previous, uint16_t next) {
  if (!have) return FIRST;
  const uint16_t delta = uint16_t(next - previous);
  if (!delta) return DUPLICATE;
  return delta < 0x8000U ? FORWARD : OLD;
}
inline bool allZero(const uint8_t *p) {
  for (uint8_t i = 0; i < DLC; ++i) if (p[i]) return false;
  return true;
}
inline uint16_t simulatedDistance(uint16_t seq) {
  const uint16_t cm[8] = {250, 200, 100, 50, 25, 50, 100, 200};
  return cm[seq % 8];
}
inline void encode(uint8_t *p, uint16_t seq, uint16_t cm,
                   uint8_t flags, uint16_t session) {
  p[0] = VERSION; put16(p + 1, seq); put16(p + 3, cm);
  p[5] = flags; put16(p + 6, session);
}
inline bool validSensor(const uint8_t *p) {
  return p[0] == VERSION && u16(p + 3) <= MAX_CM && p[5] <= 1
         && u16(p + 6) != 0;
}
inline bool validStatus(const uint8_t *p) {
  if (p[0] != VERSION || u16(p + 3) > MAX_CM || !u16(p + 6)) return false;
  const uint16_t cm = u16(p + 3);
  return p[5] == SENSOR_FAULT ||
         p[5] == (cm <= STOP_CM ? STOP : RUN);
}
}
#endif
