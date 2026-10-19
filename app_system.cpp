// Aplicativo SYSTEM: informacoes do hardware, RAM livre, uptime e recordes.
#include "gos_display.h"
#include "gos_hal.h"
#include "gos_apps.h"

static uint32_t aStart;
static uint32_t msgUntil;

void sys_init() { aStart = 0; msgUntil = 0; }

bool sys_frame() {
  uint8_t p = hal_pressed(), h = hal_held();
  uint32_t now = millis();

  if (p & BTN_UP) { hal_sound_on = !hal_sound_on; hal_sound(1000, 60); }
  if (p & BTN_B && !(h & BTN_A)) return false;

  if (h & BTN_A) {
    if (!aStart) aStart = now;
    if (now - aStart > 2000) { hal_hiscore_reset(); msgUntil = now + 1500; aStart = 0; hal_sound(300, 200); }
  } else aStart = 0;

  gfx_clear();
  gfx_fill(0, 0, 128, 9, 1);
  gfx_textP(2, 1, PSTR("SYSTEM"), 1, 0);
  gfx_textP(70, 1, PSTR("B=VOLTAR"), 1, 0);

  char b[24];
  gfx_textP(0, 12, PSTR("ATMEGA328 16MHZ 5V"));
  gfx_textP(0, 21, PSTR("FLASH 32K RAM 2K"));
  snprintf_P(b, sizeof(b), PSTR("RAM LIVRE: %dB"), hal_free_ram());
  gfx_text(0, 30, b);
  uint32_t s = now / 1000UL;
  snprintf_P(b, sizeof(b), PSTR("UPTIME: %02lu:%02lu"), (s / 60) % 100, s % 60);
  gfx_text(0, 39, b);
  snprintf_P(b, sizeof(b), PSTR("D:%u P:%u T:%u"), hal_hiscore(HS_DOOM), hal_hiscore(HS_PACMAN), hal_hiscore(HS_TETRIS));
  gfx_text(0, 48, b);

  if (now < msgUntil) gfx_textP(0, 57, PSTR("RECORDES ZERADOS!"));
  else {
    gfx_textP(0, 57, hal_sound_on ? PSTR("UP:SOM LIGADO") : PSTR("UP:SOM MUDO"));
    gfx_textP(86, 57, PSTR("A:ZERAR"));
  }
  return true;
}
