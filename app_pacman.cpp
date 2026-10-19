// PACMAN - labirinto 23x15 (tiles de 4px). So a metade esquerda do mapa fica
// na flash; a direita e espelhada em tempo de execucao.
#include "gos_display.h"
#include "gos_hal.h"
#include "gos_apps.h"

#define MW 23
#define MH 15
#define TS  4
#define MX 0
#define MY 2
#define PANX 98

// '#' parede  '.' ponto  'o' pilula  '-' porta da casa dos fantasmas  ' ' vazio
static const char MAZE[MH][13] PROGMEM = {
  "############",
  "#o..........",
  "#.###.#####.",
  "#...........",
  "#.##.#.####.",
  "#.##.#......",
  "#....#...##-",
  " ........#  ",
  "#....#...###",
  "#.##.#......",
  "#.##.#.####.",
  "#...........",
  "#.###.#####.",
  "#o..........",
  "############"
};

struct Ghost { int8_t x, y, dx, dy; uint8_t st; uint32_t t; };   // st: 0 casa, 1 saindo, 2 cacando

static uint32_t dots[MH];
static int16_t  dotCount;
static int8_t   pcx, pcy, pdx, pdy, wx, wy;
static bool     mouth;
static Ghost    gh[3];
static uint8_t  lives, level;
static uint16_t score, hi;
static uint32_t nextPac, nextGh, scaredUntil, freezeUntil, readyUntil;
static bool     over, won;

static char tile(int8_t x, int8_t y) {
  if (y < 0 || y >= MH || x < 0 || x >= MW) return '#';
  return pgm_read_byte(&MAZE[y][x <= 11 ? x : 22 - x]);
}
static bool freeT(int8_t x, int8_t y) { char c = tile(x, y); return c != '#' && c != '-'; }
static void wrapX(int8_t& x) { if (x < 0) x = MW - 1; else if (x >= MW) x = 0; }

static void resetActors() {
  pcx = 11; pcy = 11; pdx = pdy = 0; wx = wy = 0;
  uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    gh[i].x = 10 + i; gh[i].y = 7; gh[i].dx = gh[i].dy = 0; gh[i].st = 0;
    gh[i].t = now + 1500 + i * 2500;
  }
  scaredUntil = 0;
  readyUntil = now + 1500;
}

static void resetMaze() {
  dotCount = 0;
  for (uint8_t y = 0; y < MH; y++) {
    dots[y] = 0;
    for (uint8_t x = 0; x < MW; x++) {
      char c = tile(x, y);
      if (c == '.' || c == 'o') { dots[y] |= 1UL << x; dotCount++; }
    }
  }
}

void pacman_init() {
  score = 0; lives = 3; level = 1; over = false; won = false;
  hi = hal_hiscore(HS_PACMAN);
  resetMaze(); resetActors();
  nextPac = nextGh = freezeUntil = 0;
}

static void loseLife() {
  hal_sound(180, 500);
  if (lives) lives--;
  if (lives == 0) {
    over = true;
    hal_hiscore_save(HS_PACMAN, score);
    hi = hal_hiscore(HS_PACMAN);
  } else {
    resetActors();
    freezeUntil = millis() + 900;
  }
}

static void checkHits() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    if (gh[i].st == 0 || gh[i].x != pcx || gh[i].y != pcy) continue;
    if (now < scaredUntil) {                    // comeu o fantasma
      score += 200;
      gh[i].x = 10 + i; gh[i].y = 7; gh[i].st = 0; gh[i].t = now + 2500;
      hal_sound(1200, 120);
    } else { loseLife(); return; }
  }
}

static void stepPac() {
  int8_t nx = pcx + wx, ny = pcy + wy; wrapX(nx);
  if ((wx || wy) && freeT(nx, ny)) { pdx = wx; pdy = wy; }
  nx = pcx + pdx; ny = pcy + pdy; wrapX(nx);
  if ((pdx || pdy) && freeT(nx, ny)) {
    pcx = nx; pcy = ny; mouth = !mouth;
    if (dots[pcy] & (1UL << pcx)) {
      dots[pcy] &= ~(1UL << pcx);
      dotCount--; score += 10;
      if (tile(pcx, pcy) == 'o') { scaredUntil = millis() + 6000; score += 50; hal_sound(500, 100); }
      else hal_sound(mouth ? 330 : 260, 15);
      if (dotCount == 0) {
        level++; score += 100;
        resetMaze(); resetActors();
        freezeUntil = millis() + 800;
        hal_sound(1500, 300);
      }
    }
  }
}

static void stepGhost(uint8_t i) {
  Ghost& g = gh[i];
  uint32_t now = millis();
  if (g.st == 0) { if (now >= g.t) g.st = 1; return; }
  if (g.st == 1) {                              // sai pela porta
    if (g.x < 11) g.x++; else if (g.x > 11) g.x--; else if (g.y > 5) g.y--;
    if (g.x == 11 && g.y <= 5) { g.st = 2; g.dx = (i & 1) ? -1 : 1; g.dy = 0; }
    return;
  }
  static const int8_t DX[4] = { 0, 0, -1, 1 }, DY[4] = { -1, 1, 0, 0 };
  bool scared = now < scaredUntil;
  int8_t tx = pcx, ty = pcy;
  if (i == 1) { tx = pcx + pdx * 3; ty = pcy + pdy * 3; }
  if (i == 2) {
    int16_t d = abs(pcx - g.x) + abs(pcy - g.y);
    if (d < 8) { tx = 1; ty = 13; }
  }
  bool rnd = scared || random(100) < 12;
  int8_t bestD = -1; int16_t best = 32000; uint8_t cnt = 0;
  int8_t opts[4];
  for (uint8_t d = 0; d < 4; d++) {
    if (DX[d] == -g.dx && DY[d] == -g.dy) continue;     // sem meia-volta
    int8_t nx = g.x + DX[d], ny = g.y + DY[d]; wrapX(nx);
    if (!freeT(nx, ny)) continue;
    opts[cnt++] = d;
    int16_t dd = (nx - tx) * (nx - tx) + (ny - ty) * (ny - ty);
    if (dd < best) { best = dd; bestD = d; }
  }
  if (cnt == 0) { g.dx = -g.dx; g.dy = -g.dy; return; }  // beco: volta
  if (rnd) bestD = opts[random(cnt)];
  g.dx = DX[bestD]; g.dy = DY[bestD];
  g.x += g.dx; g.y += g.dy; wrapX(g.x);
}

// ---------- desenho ----------
static void sprite(int16_t x, int16_t y, const uint8_t* r) {
  for (uint8_t j = 0; j < 4; j++)
    for (uint8_t i = 0; i < 4; i++)
      if (r[j] & (8 >> i)) gfx_pixel(x + i, y + j);
}

static void drawMaze() {
  bool blink = (millis() >> 8) & 1;
  for (int8_t y = 0; y < MH; y++)
    for (int8_t x = 0; x < MW; x++) {
      char c = tile(x, y);
      int16_t px = MX + x * TS, py = MY + y * TS;
      if (c == '#') {
        if (tile(x - 1, y) != '#' && x > 0)  gfx_vline(px, py, TS);
        if (tile(x + 1, y) != '#' && x < MW - 1) gfx_vline(px + TS - 1, py, TS);
        if (tile(x, y - 1) != '#') gfx_hline(px, py, TS);
        if (tile(x, y + 1) != '#') gfx_hline(px, py + TS - 1, TS);
      } else if (c == '-') {
        gfx_hline(px, py + 1, TS);
      }
      if (dots[y] & (1UL << x)) {
        if (c == 'o') { if (blink) gfx_fill(px + 1, py + 1, 2, 2); }
        else gfx_pixel(px + 1, py + 1);
      }
    }
}

static void drawPac(int16_t x, int16_t y, int8_t dx, int8_t dy, bool open) {
  uint8_t r[4] = { 6, 15, 15, 6 };
  if (open) {
    if (dx > 0) { r[1] = 14; r[2] = 14; }
    else if (dx < 0) { r[1] = 7; r[2] = 7; }
    else if (dy < 0) { r[0] = 0; }
    else if (dy > 0) { r[3] = 0; }
  }
  sprite(x, y, r);
}

static void drawGhost(int16_t x, int16_t y, bool scared, bool flash) {
  uint8_t r[4] = { 6, 9, 15, 10 };
  if (scared) {
    r[0] = 6; r[1] = 15; r[2] = 15; r[3] = 10;
    if (!flash) for (uint8_t j = 0; j < 4; j++) r[j] &= (j & 1) ? 5 : 10;
  }
  sprite(x, y, r);
}

static void drawPanel() {
  gfx_vline(PANX - 3, 0, 64);
  gfx_textP(PANX, 1, PSTR("SCORE"));
  gfx_num(PANX, 10, score);
  gfx_textP(PANX, 22, PSTR("HI"));
  gfx_num(PANX, 31, hi);
  gfx_textP(PANX, 43, PSTR("LV"));
  gfx_num(PANX + 14, 43, level);
  for (uint8_t i = 0; i < lives; i++) drawPac(PANX + i * 7, 55, 1, 0, false);
}

static void draw() {
  uint32_t now = millis();
  gfx_clear();
  drawMaze();
  bool scared = now < scaredUntil;
  bool flash = scared && (scaredUntil - now < 1800) && ((now >> 7) & 1);
  for (uint8_t i = 0; i < 3; i++)
    drawGhost(MX + gh[i].x * TS, MY + gh[i].y * TS, scared && gh[i].st != 0, flash);
  drawPac(MX + pcx * TS, MY + pcy * TS, pdx, pdy, mouth);
  drawPanel();
  if (now < readyUntil) {
    gfx_fill(30, 38, 40, 9, 0);
    gfx_textP(32, 39, PSTR("READY!"));
  }
}

bool pacman_frame() {
  uint8_t p = hal_pressed(), h = hal_held();
  uint32_t now = millis();

  if (over) {
    draw();
    gfx_fill(4, 18, 88, 30, 0);
    gfx_rect(4, 18, 88, 30);
    gfx_textP(19, 22, PSTR("GAME OVER"));
    gfx_textP(8, 34, PSTR("A:NOVO B:SAIR"));
    if (p & BTN_A) pacman_init();
    if (p & BTN_B) return false;
    return true;
  }

  // direcao desejada (a ultima direcao apertada fica "na fila")
  if (p & BTN_UP) { wx = 0; wy = -1; } else if (p & BTN_DOWN) { wx = 0; wy = 1; }
  else if (p & BTN_LEFT) { wx = -1; wy = 0; } else if (p & BTN_RIGHT) { wx = 1; wy = 0; }
  else if (h & BTN_UP) { wx = 0; wy = -1; } else if (h & BTN_DOWN) { wx = 0; wy = 1; }
  else if (h & BTN_LEFT) { wx = -1; wy = 0; } else if (h & BTN_RIGHT) { wx = 1; wy = 0; }

  if (now >= freezeUntil && now >= readyUntil) {
    uint16_t pacMs = level > 4 ? 90 : 125 - level * 7;
    uint16_t ghMs  = (now < scaredUntil) ? 230 : (level > 6 ? 105 : 170 - level * 10);
    if (now >= nextPac) { nextPac = now + pacMs; stepPac(); checkHits(); }
    if (now >= nextGh)  { nextGh  = now + ghMs;  for (uint8_t i = 0; i < 3; i++) stepGhost(i); checkHits(); }
  }
  draw();
  return true;
}
