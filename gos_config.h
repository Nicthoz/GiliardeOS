#pragma once
// ============================================================
//  GiliardeOS - configuracao de hardware
// ============================================================
#define GOS_VERSION   "1.0"

// Display OLED 0.96" 128x64 (SSD1306, I2C): SDA = A4, SCL = A5
#define OLED_ADDR     0x3C      // se a tela ficar apagada, tente 0x3D

// Botoes (um lado no pino, outro lado no GND - usa pull-up interno)
#define PIN_UP        2
#define PIN_DOWN      3
#define PIN_LEFT      4
#define PIN_RIGHT     5
#define PIN_A         6
#define PIN_B         7

// Buzzer passivo/ativo (positivo no pino, negativo no GND)
#define PIN_BUZZER    8

#define FRAME_MS      30        // tempo minimo entre quadros
