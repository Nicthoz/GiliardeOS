#include <EEPROM.h>
#include "gos_config.h"
#include "gos_hal.h"

static uint8_t held = 0, pressed = 0, prevHeld = 0;
bool hal_sound_on = true;

void hal_input_begin() {
  const uint8_t pins[6] = { PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_A, PIN_B };
  for (uint8_t i = 0; i < 6; i++) pinMode(pins[i], INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
}

void hal_input_update() {
  uint8_t h = 0;
  if (digitalRead(PIN_UP)    == LOW) h |= BTN_UP;
  if (digitalRead(PIN_DOWN)  == LOW) h |= BTN_DOWN;
  if (digitalRead(PIN_LEFT)  == LOW) h |= BTN_LEFT;
  if (digitalRead(PIN_RIGHT) == LOW) h |= BTN_RIGHT;
  if (digitalRead(PIN_A)     == LOW) h |= BTN_A;
  if (digitalRead(PIN_B)     == LOW) h |= BTN_B;
  pressed  = h & ~prevHeld;
  prevHeld = h;
  held     = h;
}
uint8_t hal_held()    { return held; }
uint8_t hal_pressed() { return pressed; }

void hal_sound(uint16_t freq, uint16_t ms) {
  if (hal_sound_on) tone(PIN_BUZZER, freq, ms);
}

// EEPROM: [0]='G' [1]='O' [2..7] = 3 recordes de 16 bits
void hal_storage_begin() {
  if (EEPROM.read(0) != 'G' || EEPROM.read(1) != 'O') {
    EEPROM.write(0, 'G'); EEPROM.write(1, 'O');
    hal_hiscore_reset();
  }
}
uint16_t hal_hiscore(uint8_t slot) {
  uint16_t v; EEPROM.get(2 + slot * 2, v); return v;
}
void hal_hiscore_save(uint8_t slot, uint16_t v) {
  if (v > hal_hiscore(slot)) EEPROM.put(2 + slot * 2, v);
}
void hal_hiscore_reset() {
  uint16_t z = 0;
  for (uint8_t i = 0; i < 3; i++) EEPROM.put(2 + i * 2, z);
}

extern int __heap_start, *__brkval;
int hal_free_ram() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
