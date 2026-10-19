#pragma once
#include <Arduino.h>

// ---- Kernel ----
void gos_boot();
void gos_loop();

// ---- Aplicativos: cada um tem init() e frame() ----
// frame() devolve false quando o app quer voltar ao menu.
void tetris_init();  bool tetris_frame();
void pacman_init();  bool pacman_frame();
void doom_init();    bool doom_frame();
void sys_init();     bool sys_frame();
