#pragma once
#include <Arduino.h>

// ---- Botoes ----
enum : uint8_t { BTN_UP = 1, BTN_DOWN = 2, BTN_LEFT = 4, BTN_RIGHT = 8, BTN_A = 16, BTN_B = 32 };
void    hal_input_begin();
void    hal_input_update();      // chamar 1x por quadro
uint8_t hal_held();              // botoes segurados agora
uint8_t hal_pressed();           // botoes que acabaram de ser apertados neste quadro

// ---- Som ----
extern bool hal_sound_on;
void hal_sound(uint16_t freq, uint16_t ms);

// ---- Recordes na EEPROM ----
enum : uint8_t { HS_DOOM = 0, HS_PACMAN = 1, HS_TETRIS = 2 };
void     hal_storage_begin();
uint16_t hal_hiscore(uint8_t slot);
void     hal_hiscore_save(uint8_t slot, uint16_t v);   // so grava se for maior
void     hal_hiscore_reset();

// ---- Sistema ----
int hal_free_ram();
