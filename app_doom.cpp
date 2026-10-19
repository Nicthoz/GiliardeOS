// DOOM 8-bit - motor de "raycasting" em matematica inteira (sem float, rapido no AVR).
// Mapa 16x16 = 32 bytes. Inimigos desenhados como sprites escalados com z-buffer.
//
// Controles:  CIMA/BAIXO = andar   ESQ/DIR = girar
//             B + ESQ/DIR = andar de lado (strafe)   A = atirar
#include "gos_display.h"
#include "gos_hal.h"
#include "gos_apps.h"

#define VIEW_H 56          // area 3D (o resto e o HUD)
#define NCOL   64          // raios por quadro (cada um vira 2px de largura)
#define MAXN   96          // alcance maximo do raio (passos de 1/8 de celula)
#define NEN    4

// 1 = parede. Bit x da linha y.
static const uint16_t MAP[16] PROGMEM = {
  0xFFFF, 0x8001, 0xBC3D, 0xA005, 0xA425, 0x8421, 0x8661, 0x8001,
  0x8001, 0x8661, 0x8421, 0xA425, 0xA005, 0xBC3D, 0x8001, 0xFFFF
};
static const uint8_t SPAWN[6][2] PROGMEM = { {14,1}, {14,14}, {1,14}, {8,3}, {8,12}, {14,7} };

struct Enemy { int16_t x, y; int8_t hp; uint32_t atk; };

static Enemy    en[NEN];
static int16_t  px, py;            // posicao em 1/256 de celula
static uint16_t pa;                // angulo: 1024 = volta completa
static int16_t  hp;
static uint16_t kills, score, hi;
static uint8_t  wave;
static uint32_t lastT, fireReady, fireUntil, hurtUntil;
static bool     over;
static uint8_t  zb[NCOL];

// seno aproximado (Bhaskara) em Q7: devolve -127..127. Sem tabela, sem float.
static int16_t sin7(uint16_t a) {
  uint16_t t = a & 511;
  int32_t u = (int32_t)t * (512 - t);
  int16_t s = (u * 508L) / (327680L - u);
  return (a & 512) ? -s : s;
}
static int16_t cos7(uint16_t a) { return sin7(a + 256); }

static bool wallAt(int16_t cx, int16_t cy) {
  if (cx < 0 || cy < 0 || cx >= 16 || cy >= 16) return true;
  return pgm_read_word(&MAP[cy]) & (1U << cx);
}
static bool solidAt(int16_t x, int16_t y) { return wallAt(x >> 8, y >> 8); }
static bool blocked(int16_t x, int16_t y) {
  const int16_t r = 44;
  return solidAt(x - r, y - r) || solidAt(x + r, y - r) || solidAt(x - r, y + r) || solidAt(x + r, y + r);
}

static void spawnWave() {
  uint8_t n = wave + 1; if (n > NEN) n = NEN;
  for (uint8_t i = 0; i < NEN; i++) {
    if (i >= n) { en[i].hp = 0; continue; }
    uint8_t k = (i * 2 + wave) % 6;
    int16_t ex = pgm_read_byte(&SPAWN[k][0]) * 256 + 128;
    int16_t ey = pgm_read_byte(&SPAWN[k][1]) * 256 + 128;
    if (abs(ex - px) + abs(ey - py) < 1024) {
      k = (k + 3) % 6;
      ex = pgm_read_byte(&SPAWN[k][0]) * 256 + 128;
      ey = pgm_read_byte(&SPAWN[k][1]) * 256 + 128;
    }
    en[i].x = ex; en[i].y = ey;
    en[i].hp = wave > 4 ? 3 : 2;
    en[i].atk = millis() + 1500;
  }
}

void doom_init() {
  px = 2 * 256 + 128; py = 7 * 256 + 128; pa = 0;
  hp = 100; kills = 0; score = 0; wave = 1; over = false;
  hi = hal_hiscore(HS_DOOM);
  fireReady = fireUntil = hurtUntil = 0;
  lastT = millis();
  spawnWave();
}

// ---------- projecao de um inimigo na tela ----------
static bool project(const Enemy& e, int32_t& d, int32_t& sd) {
  int16_t dx = e.x - px, dy = e.y - py;
  int16_t c = cos7(pa), s = sin7(pa);
  d  = ((int32_t)dx * c + (int32_t)dy * s) >> 7;     // profundidade
  sd = ((int32_t)dy * c - (int32_t)dx * s) >> 7;     // lado (+ = direita)
  return d > 48;
}

static void shoot() {
  int8_t best = -1; int32_t bd = 32000;
  uint8_t zc = zb[31] < zb[32] ? zb[31] : zb[32];
  for (uint8_t i = 0; i < NEN; i++) {
    if (en[i].hp <= 0) continue;
    int32_t d, sd;
    if (!project(en[i], d, sd)) continue;
    if (abs(sd) < 80 && d < bd && (d >> 5) < zc) { bd = d; best = i; }
  }
  if (best >= 0) {
    if (--en[best].hp <= 0) { kills++; score += 100; hal_sound(160, 120); }
    else hal_sound(300, 40);
  }
}

// ---------- renderizacao ----------
static void vlineClip(int16_t x, int16_t y, int16_t len, uint8_t c) {
  if (y < 0) { len += y; y = 0; }
  if (y + len > VIEW_H) len = VIEW_H - y;
  if (len > 0) gfx_vline(x, y, len, c);
}

static bool shade(uint8_t nc, uint8_t x, uint8_t y) {
  if (nc < 10) return true;
  if (nc < 24) return ((x + y) & 1) == 0;
  if (nc < 48) return !(x & 1) && !(y & 1);
  return !(x & 3) && !(y & 3);
}

static void renderWalls() {
  for (uint8_t col = 0; col < NCOL; col++) {
    int16_t off = ((int16_t)col - 32) * 21 / 8;            // campo de visao ~60 graus
    uint16_t ang = ((uint16_t)pa + (uint16_t)off) & 1023;
    int16_t stx = cos7(ang) * 4, sty = sin7(ang) * 4;      // passo = ate 1/8 de celula
    int32_t rx = (int32_t)px << 4, ry = (int32_t)py << 4;  // 1/4096 de celula
    uint8_t n = 0;
    do { rx += stx; ry += sty; n++; } while (n < MAXN && !wallAt(rx >> 12, ry >> 12));

    int16_t nc = ((int16_t)n * cos7((uint16_t)off & 1023)) >> 7;   // corrige "olho de peixe"
    if (nc < 1) nc = 1;
    zb[col] = nc > 255 ? 255 : nc;
    int16_t h = 400 / nc; if (h > VIEW_H) h = VIEW_H;
    int16_t y0 = (VIEW_H - h) / 2;
    uint8_t x = col * 2;
    for (int16_t y = y0; y < y0 + h; y++) {
      if (shade(nc, x, y))     gfx_pixel(x, y);
      if (shade(nc, x + 1, y)) gfx_pixel(x + 1, y);
    }
  }
}

static void renderFloor() {
  static const uint8_t ys[5] = { 32, 36, 41, 47, 54 };
  static const uint8_t st[5] = { 10, 8, 6, 4, 3 };
  for (uint8_t i = 0; i < 5; i++)
    for (int16_t x = (i & 1) * 2; x < 128; x += st[i]) gfx_pixel(x, ys[i]);
}

static void drawEnemy(const Enemy& e) {
  int32_t d, sd;
  if (!project(e, d, sd)) return;
  int16_t sx = 64 + (int16_t)((sd * 111) / d);
  if (sx < -80 || sx > 208) return;
  int16_t w = (int16_t)(14200L / d), h = (int16_t)(10240L / d);
  if (w < 2) w = 2;
  if (h < 3) h = 3;
  int16_t yb = 28 + (int16_t)(6400L / d);        // linha do chao
  int16_t y0 = yb - h;
  uint8_t dz = (uint8_t)(d >> 5);
  int16_t x0 = sx - w / 2, x1 = sx + w / 2;
  int16_t headH = h * 3 / 8;

  for (int16_t x = x0 - 1; x <= x1 + 1; x++) {
    if (x < 0 || x >= 128 || zb[x >> 1] <= dz) continue;
    vlineClip(x, y0 - 1, h + 2, 0);              // contorno preto
    if (x < x0 || x > x1) continue;
    if (x >= x0 + w / 5 && x <= x1 - w / 5) vlineClip(x, y0, h, 1);                  // cabeca + corpo
    else vlineClip(x, y0 + headH, h - headH, 1);                                      // so corpo
  }
  if (h >= 8) {                                  // olhos e boca
    int16_t es = w / 8; if (es < 1) es = 1;
    int16_t eh = h / 10; if (eh < 1) eh = 1;
    int16_t ey = y0 + h / 8;
    for (int8_t side = -1; side <= 1; side += 2) {
      int16_t ex = sx + side * (w / 6) - (side < 0 ? es : 0);
      for (int16_t i = 0; i < es; i++)
        if (ex + i >= 0 && ex + i < 128 && zb[(ex + i) >> 1] > dz)
          for (int16_t j = 0; j < eh; j++) gfx_pixel(ex + i, ey + j, 0);
    }
    for (int16_t i = -w / 8; i <= w / 8; i++) {
      int16_t mx = sx + i;
      if (mx >= 0 && mx < 128 && zb[mx >> 1] > dz) gfx_pixel(mx, y0 + headH - 1, 0);
    }
  }
}

static void drawEnemies() {
  uint8_t done = 0;                               // desenha do mais longe para o mais perto
  for (uint8_t pass = 0; pass < NEN; pass++) {
    int8_t pick = -1; int32_t far = -1;
    for (uint8_t i = 0; i < NEN; i++) {
      if (en[i].hp <= 0 || (done & (1 << i))) continue;
      int32_t d, sd;
      project(en[i], d, sd);
      if (d > far) { far = d; pick = i; }
    }
    if (pick < 0) break;
    done |= 1 << pick;
    drawEnemy(en[pick]);
  }
}

static void drawGun(bool fire) {
  int8_t o = fire ? 3 : 0;
  gfx_fill(58, 44 + o, 12, 12 - o, 0);
  gfx_fill(62, 44 + o, 4, 7, 1);
  gfx_fill(60, 50 + o, 8, 6 - o, 1);
  gfx_fill(63, 46 + o, 2, 3, 0);
  if (fire) { gfx_fill(61, 37, 6, 5, 1); gfx_fill(63, 34, 2, 11, 1); gfx_fill(58, 39, 12, 2, 1); }
}

static void drawHud() {
  gfx_fill(0, VIEW_H, 128, 8, 0);
  gfx_hline(0, VIEW_H, 128);
  // rostinho
  gfx_fill(1, 58, 7, 6, 1);
  gfx_pixel(2, 60, 0); gfx_pixel(6, 60, 0);
  if (hp > 60) gfx_hline(3, 62, 3, 0); else if (hp > 30) gfx_pixel(4, 62, 0); else { gfx_pixel(3, 62, 0); gfx_pixel(5, 62, 0); }
  gfx_textP(11, 57, PSTR("HP"));
  gfx_rect(25, 58, 42, 6);
  int16_t w = hp > 0 ? hp * 40 / 100 : 0;
  gfx_fill(26, 59, w, 4);
  gfx_textP(72, 57, PSTR("W"));  gfx_num(78, 57, wave);
  gfx_textP(92, 57, PSTR("K"));  gfx_num(98, 57, kills);
}

bool doom_frame() {
  uint32_t now = millis();
  int16_t dt = now - lastT; lastT = now;
  if (dt < 15) dt = 15;
  if (dt > 100) dt = 100;
  uint8_t p = hal_pressed(), h = hal_held();

  if (over) {
    gfx_fill(10, 14, 108, 34, 0);
    gfx_rect(10, 14, 108, 34);
    gfx_textP(30, 19, PSTR("VOCE MORREU"));
    gfx_textP(22, 29, PSTR("SCORE"));  gfx_num(58, 29, score);
    gfx_textP(16, 39, PSTR("A:NOVO  B:SAIR"));
    if (p & BTN_A) doom_init();
    if (p & BTN_B) return false;
    return true;
  }

  // ---- movimento ----
  int16_t c = cos7(pa), s = sin7(pa);
  int32_t spd = (int32_t)dt * 3;
  int16_t mx = 0, my = 0;
  if (h & BTN_UP)   { mx += (c * spd) >> 9; my += (s * spd) >> 9; }
  if (h & BTN_DOWN) { mx -= (c * spd) >> 9; my -= (s * spd) >> 9; }
  int16_t turn = (dt * 15) >> 4;
  if (h & BTN_B) {                                    // strafe
    if (h & BTN_RIGHT) { mx -= (s * spd) >> 9; my += (c * spd) >> 9; }
    if (h & BTN_LEFT)  { mx += (s * spd) >> 9; my -= (c * spd) >> 9; }
  } else {
    if (h & BTN_RIGHT) pa = (pa + turn) & 1023;
    if (h & BTN_LEFT)  pa = (pa - turn) & 1023;
  }
  if (!blocked(px + mx, py)) px += mx;
  if (!blocked(px, py + my)) py += my;

  // ---- tiro ----
  if ((p & BTN_A) && now >= fireReady) {
    fireReady = now + 300; fireUntil = now + 100;
    hal_sound(120, 60);
    shoot();
  }

  // ---- inimigos ----
  uint8_t alive = 0;
  int16_t sp = (dt * 3) >> 4;
  for (uint8_t i = 0; i < NEN; i++) {
    Enemy& e = en[i];
    if (e.hp <= 0) continue;
    alive++;
    int16_t dx = px - e.x, dy = py - e.y;
    int16_t man = abs(dx) + abs(dy);
    if (man < 2600 && man > 330) {
      int16_t sx = dx > sp ? sp : (dx < -sp ? -sp : dx);
      int16_t sy = dy > sp ? sp : (dy < -sp ? -sp : dy);
      if (!blocked(e.x + sx, e.y)) e.x += sx;
      if (!blocked(e.x, e.y + sy)) e.y += sy;
    }
    if (man <= 400 && now >= e.atk) {
      e.atk = now + 700;
      hp -= 10;
      hurtUntil = now + 120;
      hal_sound(90, 120);
    }
  }
  if (hp <= 0) {
    hp = 0; over = true;
    hal_hiscore_save(HS_DOOM, score);
    hi = hal_hiscore(HS_DOOM);
    hal_sound(100, 500);
  }
  if (!alive) {                                     // onda limpa
    wave++; score += 50;
    hp += 20; if (hp > 100) hp = 100;
    spawnWave();
    hal_sound(1200, 200);
  }

  // ---- desenho ----
  gfx_clear();
  renderFloor();
  renderWalls();
  drawEnemies();
  drawGun(now < fireUntil);
  gfx_pixel(62, 28, 2); gfx_pixel(66, 28, 2); gfx_pixel(64, 26, 2); gfx_pixel(64, 30, 2);   // mira
  drawHud();
  if (now < hurtUntil) gfx_invert_view(7);
  return true;
}
