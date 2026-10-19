// Kernel do GiliardeOS: boot, launcher (menu) e gerenciador de aplicativos.
#include "gos_config.h"
#include "gos_display.h"
#include "gos_hal.h"
#include "gos_apps.h"

// ---------- tabela de aplicativos ----------
struct App {
  const char* name;      // PROGMEM
  const char* line1;     // PROGMEM
  const char* line2;     // PROGMEM
  void (*init)();
  bool (*frame)();
};

static const char APPNAME_0[] PROGMEM = "DOOM";
static const char APPDESC_0a[] PROGMEM = "SHOOTER 3D";
static const char APPDESC_0b[] PROGMEM = "MATE DEMONIOS";
static const char APPNAME_1[] PROGMEM = "PACMAN";
static const char APPDESC_1a[] PROGMEM = "COMA PONTOS";
static const char APPDESC_1b[] PROGMEM = "EVITE GHOSTS";
static const char APPNAME_2[] PROGMEM = "TETRIS";
static const char APPDESC_2a[] PROGMEM = "ENCAIXE BLOCOS";
static const char APPDESC_2b[] PROGMEM = "LIMPE LINHAS";
static const char APPNAME_3[] PROGMEM = "SYSTEM";
static const char APPDESC_3a[] PROGMEM = "INFORMACOES";
static const char APPDESC_3b[] PROGMEM = "E RECORDES";

static const App APPS[] PROGMEM = {
  { APPNAME_0, APPDESC_0a, APPDESC_0b, doom_init,   doom_frame   },
  { APPNAME_1, APPDESC_1a, APPDESC_1b, pacman_init, pacman_frame },
  { APPNAME_2, APPDESC_2a, APPDESC_2b, tetris_init, tetris_frame },
  { APPNAME_3, APPDESC_3a, APPDESC_3b, sys_init,    sys_frame    },
};
#define NAPPS (sizeof(APPS) / sizeof(APPS[0]))

static App     curApp;
static int8_t  running = -1;     // -1 = menu
static uint8_t sel = 0;
static uint32_t lastFrame = 0, comboAt = 0;
static bool    comboOn = false;

// ---------- boot ----------
static const char BOOTLINE_0[] PROGMEM = "GILIARDEOS V1.0 BOOT";
static const char BOOTLINE_1[] PROGMEM = "[ OK ] CPU 16MHZ 8BIT";
static const char BOOTLINE_2[] PROGMEM = "[ OK ] 2K RAM 32K ROM";
static const char BOOTLINE_3[] PROGMEM = "[ OK ] OLED 128X64";
static const char BOOTLINE_4[] PROGMEM = "[ OK ] BOTOES + SOM";
static const char BOOTLINE_5[] PROGMEM = "[ OK ] EEPROM 1K";
static const char BOOTLINE_6[] PROGMEM = "[ OK ] 4 APPS PRONTOS";
static const char BOOTLINE_7[] PROGMEM = "INICIANDO INTERFACE..";
static const char* const BOOT[] PROGMEM = { BOOTLINE_0, BOOTLINE_1, BOOTLINE_2, BOOTLINE_3, BOOTLINE_4, BOOTLINE_5, BOOTLINE_6, BOOTLINE_7 };

void gos_boot() {
  hal_input_begin();
  hal_storage_begin();
  gfx_begin();
  randomSeed(analogRead(A0) ^ micros());

  // log de boot estilo Linux
  for (uint8_t i = 0; i < 8; i++) {
    gfx_clear();
    for (uint8_t j = 0; j <= i; j++)
      gfx_textP(0, j * 8, (const char*)pgm_read_word(&BOOT[j]));
    gfx_show();
    if (i > 0) hal_sound(600 + i * 120, 40);
    delay(i == 0 ? 500 : 260);
  }

  // tela de abertura
  for (uint8_t p = 0; p <= 100; p += 5) {
    gfx_clear();
    gfx_rect(0, 0, 128, 64);
    gfx_textP(4, 14, PSTR("GILIARDEOS"), 2);
    gfx_hline(4, 31, 118);
    gfx_textP(25, 36, PSTR("SISTEMA 8-BIT"));
    gfx_textP(13, 46, PSTR("FEIRA DE CIENCIAS"));
    gfx_rect(14, 56, 100, 5);
    gfx_fill(14, 56, p, 5);
    gfx_show();
    delay(35);
  }
  hal_sound(880, 80); delay(90); hal_sound(1320, 140); delay(400);
}

// ---------- icones desenhados por codigo ----------
static void block(int16_t x, int16_t y) { gfx_fill(x, y, 6, 6, 1); }

static void icon(uint8_t id, int16_t cx, int16_t cy) {
  switch (id) {
    case 0:  // caveira (DOOM)
      gfx_circle_fill(cx, cy - 3, 10, 1);
      gfx_fill(cx - 6, cy + 4, 13, 7, 1);
      gfx_circle_fill(cx - 4, cy - 4, 3, 0);
      gfx_circle_fill(cx + 4, cy - 4, 3, 0);
      gfx_fill(cx - 1, cy + 1, 3, 3, 0);
      for (int8_t i = -2; i <= 2; i += 2) gfx_vline(cx + i, cy + 7, 4, 0);
      break;
    case 1:  // pacman
      gfx_circle_fill(cx - 3, cy, 10, 1);
      for (int8_t dx = 0; dx <= 11; dx++) {
        int8_t hh = dx * 3 / 5;
        gfx_vline(cx - 3 + dx, cy - hh, hh * 2 + 1, 0);
      }
      gfx_fill(cx + 11, cy - 1, 2, 2, 1);
      gfx_fill(cx + 16, cy - 1, 2, 2, 1);
      break;
    case 2:  // blocos do tetris
      block(cx - 10, cy - 11); block(cx - 3, cy - 11); block(cx + 4, cy - 11); block(cx - 3, cy - 4);
      block(cx - 10, cy + 3);  block(cx - 3, cy + 3);  block(cx + 4, cy + 3);  block(cx + 4, cy + 10);
      break;
    default: // engrenagem
      gfx_circle_fill(cx, cy, 9, 1);
      gfx_fill(cx - 2, cy - 13, 4, 5, 1); gfx_fill(cx - 2, cy + 9, 4, 5, 1);
      gfx_fill(cx - 13, cy - 2, 5, 4, 1); gfx_fill(cx + 9, cy - 2, 5, 4, 1);
      gfx_fill(cx - 10, cy - 10, 3, 3, 1); gfx_fill(cx + 8, cy - 10, 3, 3, 1);
      gfx_fill(cx - 10, cy + 8, 3, 3, 1);  gfx_fill(cx + 8, cy + 8, 3, 3, 1);
      gfx_circle_fill(cx, cy, 3, 0);
      break;
  }
}

static void launch(uint8_t i) {
  memcpy_P(&curApp, &APPS[i], sizeof(App));
  randomSeed(micros());
  curApp.init();
  running = i;
  comboOn = false;
  hal_sound(988, 60);
}

// ---------- menu / launcher ----------
static void menu_frame() {
  uint8_t p = hal_pressed();
  if (p & (BTN_RIGHT | BTN_DOWN)) { sel = (sel + 1) % NAPPS; hal_sound(700, 20); }
  if (p & (BTN_LEFT | BTN_UP))    { sel = (sel + NAPPS - 1) % NAPPS; hal_sound(700, 20); }
  if (p & BTN_A) { launch(sel); return; }

  App a; memcpy_P(&a, &APPS[sel], sizeof(App));
  gfx_clear();

  // barra de titulo
  gfx_fill(0, 0, 128, 9, 1);
  gfx_textP(2, 1, PSTR("GILIARDEOS"), 1, 0);
  gfx_textP(104, 1, PSTR("V" GOS_VERSION), 1, 0);

  // icone grande
  gfx_rect(3, 12, 42, 40);
  icon(sel, 24, 32);

  // nome e descricao
  gfx_textP(50, 15, a.name, 2);
  gfx_hline(50, 31, 76);
  gfx_textP(50, 36, a.line1);
  gfx_textP(50, 45, a.line2);

  // rodape
  gfx_textP(2, 56, PSTR("A=ABRIR"));
  for (uint8_t i = 0; i < NAPPS; i++) {
    if (i == sel) gfx_fill(70 + i * 9, 57, 5, 5); else gfx_rect(70 + i * 9, 57, 5, 5);
  }
  if ((millis() >> 8) & 1) { gfx_textP(110, 56, PSTR("<")); gfx_textP(120, 56, PSTR(">")); }
}

// ---------- laco principal ----------
void gos_loop() {
  uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  hal_input_update();

  if (running < 0) {
    menu_frame();
  } else {
    // A + B segurados por 0,6 s = sair para o menu
    if ((hal_held() & (BTN_A | BTN_B)) == (BTN_A | BTN_B)) {
      if (!comboOn) { comboOn = true; comboAt = now; }
    } else comboOn = false;

    bool alive = curApp.frame();

    if (comboOn) {
      uint32_t t = now - comboAt;
      gfx_fill(0, 62, (int16_t)(t * 128 / 600), 2, 2);
      if (t >= 600) alive = false;
    }
    if (!alive) { running = -1; comboOn = false; hal_sound(500, 60); }
  }
  gfx_show();
}
