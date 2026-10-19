/*
 *  GiliardeOS v1.0  -  mini sistema operacional 8-bit para Arduino (ATmega328)
 *  Hardware: Arduino UNO/Nano + OLED 0.96" 128x64 (SSD1306) + 6 botoes + buzzer
 *
 *  Arquitetura (parecida com um SO de verdade):
 *    gos_display  -> "driver de video"  (SSD1306 + graficos + fonte)
 *    gos_hal      -> "drivers" de botoes, som e EEPROM (recordes)
 *    gos_kernel   -> boot, tela inicial (launcher) e gerenciador de apps
 *    app_*        -> aplicativos: DOOM, PACMAN, TETRIS e SYSTEM
 *
 *  Para sair de qualquer app: segure A + B por meio segundo.
 */
#include "gos_config.h"
#include "gos_display.h"
#include "gos_hal.h"
#include "gos_apps.h"

void setup() { gos_boot(); }
void loop()  { gos_loop(); }
