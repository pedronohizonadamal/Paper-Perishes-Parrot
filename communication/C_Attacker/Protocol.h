#ifndef CANLAB_ATTACK_PROTOCOL_H
#define CANLAB_ATTACK_PROTOCOL_H

#include <stdint.h>

namespace Lab {
const uint16_t SENSOR_ID = 0x120;
const uint8_t VERSION = 1;
const uint8_t DLC = 8;
const uint16_t MAX_CM = 5000;
enum Order : uint8_t { FIRST, FORWARD, DUPLICATE, OLD };

inline uint16_t u16(const uint8_t *p) {
  return (uint16_t(p[0]) << 8) | p[1];
}
inline void put16(uint8_t *p, uint16_t value) {
  p[0] = uint8_t(value >> 8);
  p[1] = uint8_t(value);
}
inline bool elapsed(uint32_t now, uint32_t then, uint32_t interval) {
  return uint32_t(now - then) >= interval;
}
inline bool deadlineReached(uint32_t now, uint32_t deadline) {
  return int32_t(now - deadline) >= 0;
}
inline Order order(bool have, uint16_t previous, uint16_t next) {
  if (!have) return FIRST;
  const uint16_t delta = uint16_t(next - previous);
  if (!delta) return DUPLICATE;
  return delta < 0x8000U ? FORWARD : OLD;
}
inline bool allZero(const uint8_t *data) {
  for (uint8_t i = 0; i < DLC; ++i) if (data[i]) return false;
  return true;
}
inline bool validSensor(const uint8_t *data) {
  return data[0] == VERSION && u16(data + 3) <= MAX_CM &&
         data[5] <= 1 && u16(data + 6) != 0;
}
inline void encodeSensor(uint8_t *data, uint16_t sequence, uint16_t cm,
                         uint16_t session) {
  data[0] = VERSION;
  put16(data + 1, sequence);
  put16(data + 3, cm);
  data[5] = 0;
  put16(data + 6, session);
}
}

#endif
