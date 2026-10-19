#pragma once
#include <Arduino.h>

#define SCR_W 128
#define SCR_H 64

// cor: 0 = apaga, 1 = acende, 2 = inverte (XOR)
void gfx_begin();
void gfx_clear();
void gfx_show();                                   // envia o quadro para a tela
void gfx_invert_view(uint8_t pages);               // inverte as N primeiras paginas (8px cada)

void gfx_pixel(int16_t x, int16_t y, uint8_t c = 1);
void gfx_hline(int16_t x, int16_t y, int16_t w, uint8_t c = 1);
void gfx_vline(int16_t x, int16_t y, int16_t h, uint8_t c = 1);
void gfx_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c = 1);
void gfx_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t c = 1);
void gfx_circle_fill(int16_t cx, int16_t cy, int8_t r, uint8_t c = 1);

// Texto: fonte 5x7 (maiusculas, numeros e pontuacao basica)
void gfx_text (int16_t x, int16_t y, const char* s, uint8_t scale = 1, uint8_t c = 1);   // texto na RAM
void gfx_textP(int16_t x, int16_t y, const char* s, uint8_t scale = 1, uint8_t c = 1);   // texto na flash (PSTR)
void gfx_num  (int16_t x, int16_t y, int32_t n,     uint8_t scale = 1, uint8_t c = 1);
