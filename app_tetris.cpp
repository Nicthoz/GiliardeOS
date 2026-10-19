// TETRIS - tabuleiro 10x16, celulas de 4px. O tabuleiro cabe em 32 bytes de RAM
// (uma linha = 1 inteiro de 16 bits, 1 bit por coluna).
#include "gos_display.h"
#include "gos_hal.h"
#include "gos_apps.h"

#define TW   10
#define TH   16
#define CELL 4
#define BX   14      // x do tabuleiro na tela
#define PX   64      // x do painel lateral

// 7 pecas em grade 4x4 (bit = linha*4 + coluna) e o tamanho da grade p/ rotacao
static const uint16_t PIECE_M[7] PROGMEM = { 0x00F0, 0x0033, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074 }; // I O T S Z J L
static const uint8_t  PIECE_N[7] PROGMEM = { 4, 2, 3, 3, 3, 3, 3 };

static uint16_t board[TH];
static int8_t   px, py;
static uint8_t  cur, rot, nxt, level;
static uint16_t score, lines, hi;
static uint32_t tDrop, tMove;
static bool     over;

static uint16_t shapeOf(uint8_t t, uint8_t r) {
  uint16_t m = pgm_read_word(&PIECE_M[t]);
  uint8_t  n = pgm_read_byte(&PIECE_N[t]);
  while (r--) {
    uint16_t o = 0;
    for (uint8_t y = 0; y < n; y++)
      for (uint8_t x = 0; x < n; x++)
        if (m & (1U << ((n - 1 - x) * 4 + y))) o |= 1U << (y * 4 + x);
    m = o;
  }
  return m;
}

static bool collide(uint16_t m, int8_t x, int8_t y) {
  for (uint8_t i = 0; i < 16; i++) {
    if (!(m & (1U << i))) continue;
    int8_t bx = x + (i & 3), by = y + (i >> 2);
    if (bx < 0 || bx >= TW || by >= TH) return true;
    if (by >= 0 && (board[by] & (1U << bx))) return true;
  }
  return false;
}

static uint16_t gravityMs() {
  int16_t g = 720 - (int16_t)level * 60;
  return g < 90 ? 90 : g;
}

static void spawn() {
  cur = nxt; nxt = random(7);
  rot = 0; px = 3; py = 0;
  tDrop = millis();
  if (collide(shapeOf(cur, rot), px, py)) {
    over = true;
    hal_hiscore_save(HS_TETRIS, score);
    hi = hal_hiscore(HS_TETRIS);
    hal_sound(200, 400);
  }
}

static void lockPiece() {
  uint16_t m = shapeOf(cur, rot);
  for (uint8_t i = 0; i < 16; i++)
    if (m & (1U << i)) {
      int8_t by = py + (i >> 2), bx = px + (i & 3);
      if (by >= 0) board[by] |= 1U << bx;
    }
  uint8_t cleared = 0;
  for (int8_t y = TH - 1; y >= 0; y--) {
    if (board[y] == 0x03FF) {
      for (int8_t k = y; k > 0; k--) board[k] = board[k - 1];
      board[0] = 0;
      cleared++; y++;               // reanalisa a mesma linha
    }
  }
  if (cleared) {
    static const uint8_t pts[5] = { 0, 10, 30, 60, 100 };
    score += pts[cleared] * level;
    lines += cleared;
    level = 1 + lines / 10;
    hal_sound(900 + cleared * 150, 120);
  } else hal_sound(250, 30);
  spawn();
}

void tetris_init() {
  for (uint8_t i = 0; i < TH; i++) board[i] = 0;
  score = 0; lines = 0; level = 1; over = false;
  hi = hal_hiscore(HS_TETRIS);
  nxt = random(7);
  tMove = 0;
  spawn();
}

static void cell(int16_t x, int16_t y) { gfx_fill(x, y, 3, 3, 1); }

static void draw() {
  gfx_clear();
  gfx_vline(BX - 2, 0, 64);
  gfx_vline(BX + TW * CELL + 1, 0, 64);

  for (uint8_t y = 0; y < TH; y++)
    for (uint8_t x = 0; x < TW; x++)
      if (board[y] & (1U << x)) cell(BX + x * CELL, y * CELL);

  if (!over) {
    uint16_t m = shapeOf(cur, rot);
    // sombra (onde a peca vai cair)
    int8_t gy = py;
    while (!collide(m, px, gy + 1)) gy++;
    for (uint8_t i = 0; i < 16; i++)
      if (m & (1U << i)) {
        gfx_pixel(BX + (px + (i & 3)) * CELL + 1, (gy + (i >> 2)) * CELL + 1);
        cell(BX + (px + (i & 3)) * CELL, (py + (i >> 2)) * CELL);
      }
  }

  // painel
  gfx_textP(PX, 0, PSTR("NEXT"));
  uint16_t nm = shapeOf(nxt, 0);
  for (uint8_t i = 0; i < 16; i++)
    if (nm & (1U << i)) cell(PX + (i & 3) * CELL, 10 + (i >> 2) * CELL);
  gfx_textP(PX, 30, PSTR("SCORE"));
  gfx_num(PX, 39, score);
  gfx_textP(PX, 48, PSTR("LV"));  gfx_num(PX + 14, 48, level);
  gfx_textP(PX + 34, 48, PSTR("LN")); gfx_num(PX + 48, 48, lines);
  gfx_textP(PX, 57, PSTR("HI"));  gfx_num(PX + 14, 57, hi);
}

bool tetris_frame() {
  uint8_t p = hal_pressed(), h = hal_held();
  uint32_t now = millis();

  if (over) {
    draw();
    gfx_fill(8, 18, 112, 30, 0);
    gfx_rect(8, 18, 112, 30);
    gfx_textP(37, 23, PSTR("GAME OVER"));
    gfx_textP(14, 34, PSTR("A:DE NOVO B:SAIR"));
    if (p & BTN_A) tetris_init();
    if (p & BTN_B) return false;
    return true;
  }

  // mover esquerda / direita (com repeticao)
  int8_t dir = 0;
  if (p & BTN_LEFT)       { dir = -1; tMove = now + 170; }
  else if (p & BTN_RIGHT) { dir =  1; tMove = now + 170; }
  else if ((h & BTN_LEFT)  && now >= tMove) { dir = -1; tMove = now + 70; }
  else if ((h & BTN_RIGHT) && now >= tMove) { dir =  1; tMove = now + 70; }
  if (dir && !collide(shapeOf(cur, rot), px + dir, py)) { px += dir; hal_sound(450, 10); }

  // girar (com pequenos "kicks" nas paredes)
  if (p & (BTN_UP | BTN_A)) {
    uint8_t nr = (rot + 1) & 3;
    uint16_t m = shapeOf(cur, nr);
    static const int8_t kick[5] = { 0, -1, 1, -2, 2 };
    for (uint8_t k = 0; k < 5; k++)
      if (!collide(m, px + kick[k], py)) { px += kick[k]; rot = nr; hal_sound(700, 15); break; }
  }

  // queda rapida
  if (p & BTN_B) {
    uint16_t m = shapeOf(cur, rot);
    while (!collide(m, px, py + 1)) { py++; score++; }
    lockPiece();
    draw();
    return true;
  }

  // gravidade / queda suave
  uint16_t gap = (h & BTN_DOWN) ? 45 : gravityMs();
  if (now - tDrop >= gap) {
    tDrop = now;
    if (!collide(shapeOf(cur, rot), px, py + 1)) { py++; if (h & BTN_DOWN) score++; }
    else lockPiece();
  }
  draw();
  return true;
}
