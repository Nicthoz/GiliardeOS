#!/usr/bin/env python3
"""
GiliardeOS Simulator  -  testa o GiliardeOS no PC, sem Arduino.

  python gos_sim.py            -> abre a janela do "OLED" + terminal de comandos
  python gos_sim.py --test     -> teste automatico (sem janela) de todos os apps

Teclas na janela:  setas = CIMA/BAIXO/ESQ/DIR   Z = botao A   X = botao B
                   Esc = sair do app (igual a segurar A+B)

Comandos no terminal (digite 'help'):
  info | run <app> | menu | scores | reset | sound on/off | ascii | save | exit
Requisitos: Python 3.8+ (tkinter ja vem junto no Windows/macOS; no Linux: sudo apt install python3-tk)
"""
import math, random, time, json, os, sys, threading, queue

FONT = bytes([
0,0,0,0,0,0,0,95,0,0,0,7,0,7,0,20,127,20,127,20,36,42,127,42,18,35,19,8,100,98,54,73,85,34,80,0,5,3,0,0,0,28,34,65,0,0,65,34,28,0,20,8,62,8,20,8,8,62,8,8,0,80,48,0,0,8,8,8,8,8,0,96,96,0,0,32,16,8,4,2,62,81,73,69,62,0,66,127,64,0,66,97,81,73,70,33,65,69,75,49,24,20,18,127,16,39,69,69,69,57,60,74,73,73,48,1,113,9,5,3,54,73,73,73,54,6,73,73,41,30,0,54,54,0,0,0,86,54,0,0,8,20,34,65,0,20,20,20,20,20,0,65,34,20,8,2,1,81,9,6,50,73,121,65,62,126,17,17,17,126,127,73,73,73,54,62,65,65,65,34,127,65,65,34,28,127,73,73,73,65,127,9,9,9,1,62,65,73,73,122,127,8,8,8,127,0,65,127,65,0,32,64,65,63,1,127,8,20,34,65,127,64,64,64,64,127,2,12,2,127,127,4,8,16,127,62,65,65,65,62,127,9,9,9,6,62,65,81,33,94,127,9,25,41,70,70,73,73,73,49,1,1,127,1,1,63,64,64,64,63,31,32,64,32,31,63,64,56,64,63,99,20,8,20,99,7,8,112,8,7,97,81,73,69,67
])

W, H = 128, 64
EEPROM_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "giliardeos_eeprom.json")

# ======================================================================
#  "driver de video" - mesmo conjunto de funcoes gfx_* do Arduino
# ======================================================================
class Gfx:
    def __init__(s):
        s.fb = bytearray(W * H)
    def clear(s):
        s.fb[:] = bytes(W * H)
    def pixel(s, x, y, c=1):
        x = int(x); y = int(y)
        if 0 <= x < W and 0 <= y < H:
            i = y * W + x
            if c == 2: s.fb[i] ^= 1
            else: s.fb[i] = c
    def hline(s, x, y, w, c=1):
        for i in range(int(w)): s.pixel(x + i, y, c)
    def vline(s, x, y, h, c=1):
        for i in range(int(h)): s.pixel(x, y + i, c)
    def rect(s, x, y, w, h, c=1):
        s.hline(x, y, w, c); s.hline(x, y + h - 1, w, c); s.vline(x, y, h, c); s.vline(x + w - 1, y, h, c)
    def fill(s, x, y, w, h, c=1):
        for j in range(int(h)): s.hline(x, y + j, w, c)
    def circle(s, cx, cy, r, c=1):
        for y in range(-r, r + 1):
            for x in range(-r, r + 1):
                if x * x + y * y <= r * r + r: s.pixel(cx + x, cy + y, c)
    def invert_view(s, pages):
        for i in range(pages * W * 8): s.fb[i] ^= 1
    def text(s, x, y, t, scale=1, c=1):
        for ch in str(t).upper():
            o = ord(ch)
            if o < 32 or o > 90: o = 63
            g = FONT[(o - 32) * 5:(o - 32) * 5 + 5]
            for col in range(5):
                for row in range(7):
                    if g[col] & (1 << row):
                        if scale == 1: s.pixel(x + col, y + row, c)
                        else: s.fill(x + col * scale, y + row * scale, scale, scale, c)
            x += 6 * scale

BTNS = ("UP", "DOWN", "LEFT", "RIGHT", "A", "B")

class Hal:
    """botoes, som e EEPROM (arquivo json)"""
    def __init__(s, clock):
        s.clock = clock; s.held = set(); s.prev = set(); s.pressed = set(); s.sound_on = True; s.last_beep = None
        try: s.hs = json.load(open(EEPROM_FILE))
        except Exception: s.hs = {"doom": 0, "pacman": 0, "tetris": 0}
    def update(s):
        s.pressed = s.held - s.prev; s.prev = set(s.held)
    def sound(s, f, ms):
        if s.sound_on: s.last_beep = (f, ms)
    def save(s):
        try: json.dump(s.hs, open(EEPROM_FILE, "w"))
        except Exception: pass
    def hi(s, k): return s.hs.get(k, 0)
    def save_hi(s, k, v):
        if v > s.hs.get(k, 0): s.hs[k] = v; s.save()
    def reset(s):
        s.hs = {"doom": 0, "pacman": 0, "tetris": 0}; s.save()

# ======================================================================
#  TETRIS
# ======================================================================
class Tetris:
    name = "TETRIS"; d1 = "ENCAIXE BLOCOS"; d2 = "LIMPE LINHAS"
    TW, TH, CELL, BX, PX = 10, 16, 4, 14, 64
    PM = [0x00F0, 0x0033, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074]
    PN = [4, 2, 3, 3, 3, 3, 3]
    def init(s, o):
        s.board = [0] * 16; s.score = 0; s.lines = 0; s.level = 1; s.over = False
        s.hi = o.hal.hi("tetris"); s.nxt = random.randrange(7); s.tmove = 0; s.spawn(o)
    def shape(s, t, r):
        m = s.PM[t]; n = s.PN[t]
        for _ in range(r):
            out = 0
            for y in range(n):
                for x in range(n):
                    if m & (1 << ((n - 1 - x) * 4 + y)): out |= 1 << (y * 4 + x)
            m = out
        return m
    def collide(s, m, x, y):
        for i in range(16):
            if m & (1 << i):
                bx = x + (i & 3); by = y + (i >> 2)
                if bx < 0 or bx >= 10 or by >= 16: return True
                if by >= 0 and s.board[by] & (1 << bx): return True
        return False
    def spawn(s, o):
        s.cur = s.nxt; s.nxt = random.randrange(7); s.rot = 0; s.px = 3; s.py = 0; s.tdrop = o.now()
        if s.collide(s.shape(s.cur, 0), s.px, s.py):
            s.over = True; o.hal.save_hi("tetris", s.score); s.hi = o.hal.hi("tetris"); o.hal.sound(200, 400)
    def lock(s, o):
        m = s.shape(s.cur, s.rot)
        for i in range(16):
            if m & (1 << i):
                by = s.py + (i >> 2); bx = s.px + (i & 3)
                if by >= 0: s.board[by] |= 1 << bx
        cleared = 0; y = 15
        while y >= 0:
            if s.board[y] == 0x3FF:
                for k in range(y, 0, -1): s.board[k] = s.board[k - 1]
                s.board[0] = 0; cleared += 1
            else: y -= 1
        if cleared:
            s.score += (0, 10, 30, 60, 100)[cleared] * s.level; s.lines += cleared
            s.level = 1 + s.lines // 10; o.hal.sound(900 + cleared * 150, 120)
        else: o.hal.sound(250, 30)
        s.spawn(o)
    def cell(s, g, x, y): g.fill(x, y, 3, 3)
    def draw(s, g):
        g.clear(); g.vline(s.BX - 2, 0, 64); g.vline(s.BX + 41, 0, 64)
        for y in range(16):
            for x in range(10):
                if s.board[y] & (1 << x): s.cell(g, s.BX + x * 4, y * 4)
        if not s.over:
            m = s.shape(s.cur, s.rot); gy = s.py
            while not s.collide(m, s.px, gy + 1): gy += 1
            for i in range(16):
                if m & (1 << i):
                    g.pixel(s.BX + (s.px + (i & 3)) * 4 + 1, (gy + (i >> 2)) * 4 + 1)
                    s.cell(g, s.BX + (s.px + (i & 3)) * 4, (s.py + (i >> 2)) * 4)
        P = s.PX
        g.text(P, 0, "NEXT"); nm = s.shape(s.nxt, 0)
        for i in range(16):
            if nm & (1 << i): s.cell(g, P + (i & 3) * 4, 10 + (i >> 2) * 4)
        g.text(P, 30, "SCORE"); g.text(P, 39, s.score)
        g.text(P, 48, "LV"); g.text(P + 14, 48, s.level); g.text(P + 34, 48, "LN"); g.text(P + 48, 48, s.lines)
        g.text(P, 57, "HI"); g.text(P + 14, 57, s.hi)
    def frame(s, o):
        g = o.gfx; p = o.hal.pressed; h = o.hal.held; now = o.now()
        if s.over:
            s.draw(g); g.fill(8, 18, 112, 30, 0); g.rect(8, 18, 112, 30)
            g.text(37, 23, "GAME OVER"); g.text(14, 34, "A:DE NOVO B:SAIR")
            if "A" in p: s.init(o)
            if "B" in p: return False
            return True
        d = 0
        if "LEFT" in p: d = -1; s.tmove = now + 170
        elif "RIGHT" in p: d = 1; s.tmove = now + 170
        elif "LEFT" in h and now >= s.tmove: d = -1; s.tmove = now + 70
        elif "RIGHT" in h and now >= s.tmove: d = 1; s.tmove = now + 70
        if d and not s.collide(s.shape(s.cur, s.rot), s.px + d, s.py): s.px += d; o.hal.sound(450, 10)
        if "UP" in p or "A" in p:
            nr = (s.rot + 1) & 3; m = s.shape(s.cur, nr)
            for k in (0, -1, 1, -2, 2):
                if not s.collide(m, s.px + k, s.py): s.px += k; s.rot = nr; o.hal.sound(700, 15); break
        if "B" in p:
            m = s.shape(s.cur, s.rot)
            while not s.collide(m, s.px, s.py + 1): s.py += 1; s.score += 1
            s.lock(o); s.draw(g); return True
        gap = 45 if "DOWN" in h else max(90, 720 - s.level * 60)
        if now - s.tdrop >= gap:
            s.tdrop = now
            if not s.collide(s.shape(s.cur, s.rot), s.px, s.py + 1):
                s.py += 1
                if "DOWN" in h: s.score += 1
            else: s.lock(o)
        s.draw(g); return True

# ======================================================================
#  PACMAN
# ======================================================================
class Pacman:
    name = "PACMAN"; d1 = "COMA PONTOS"; d2 = "EVITE GHOSTS"
    MW, MH, T, MX, MY, PANX = 23, 15, 4, 0, 2, 98
    MAZE = ["############", "#o..........", "#.###.#####.", "#...........", "#.##.#.####.", "#.##.#......",
            "#....#...##-", " ........#  ", "#....#...###", "#.##.#......", "#.##.#.####.", "#...........",
            "#.###.#####.", "#o..........", "############"]
    def tile(s, x, y):
        if y < 0 or y >= 15 or x < 0 or x >= 23: return "#"
        return s.MAZE[y][x if x <= 11 else 22 - x]
    def free(s, x, y): return s.tile(x, y) not in "#-"
    def init(s, o):
        s.score = 0; s.lives = 3; s.level = 1; s.over = False; s.hi = o.hal.hi("pacman")
        s.mouth = False; s.reset_maze(); s.reset_actors(o); s.nextpac = s.nextgh = s.freeze = 0
    def reset_maze(s):
        s.dots = [[False] * 23 for _ in range(15)]; s.count = 0
        for y in range(15):
            for x in range(23):
                if s.tile(x, y) in ".o": s.dots[y][x] = True; s.count += 1
    def reset_actors(s, o):
        s.px, s.py, s.pdx, s.pdy, s.wx, s.wy = 11, 11, 0, 0, 0, 0
        now = o.now(); s.gh = []
        for i in range(3): s.gh.append({"x": 10 + i, "y": 7, "dx": 0, "dy": 0, "st": 0, "t": now + 1500 + i * 2500})
        s.scared = 0; s.ready = now + 1500
    def lose(s, o):
        o.hal.sound(180, 500); s.lives = max(0, s.lives - 1)
        if s.lives == 0: s.over = True; o.hal.save_hi("pacman", s.score); s.hi = o.hal.hi("pacman")
        else: s.reset_actors(o); s.freeze = o.now() + 900
    def hits(s, o):
        now = o.now()
        for i, g in enumerate(s.gh):
            if g["st"] == 0 or g["x"] != s.px or g["y"] != s.py: continue
            if now < s.scared:
                s.score += 200; g.update(x=10 + i, y=7, st=0, t=now + 2500); o.hal.sound(1200, 120)
            else: s.lose(o); return
    def step_pac(s, o):
        nx = (s.px + s.wx) % 23; ny = s.py + s.wy
        if (s.wx or s.wy) and s.free(nx, ny): s.pdx, s.pdy = s.wx, s.wy
        nx = (s.px + s.pdx) % 23; ny = s.py + s.pdy
        if (s.pdx or s.pdy) and s.free(nx, ny):
            s.px, s.py = nx, ny; s.mouth = not s.mouth
            if s.dots[s.py][s.px]:
                s.dots[s.py][s.px] = False; s.count -= 1; s.score += 10
                if s.tile(s.px, s.py) == "o": s.scared = o.now() + 6000; s.score += 50; o.hal.sound(500, 100)
                else: o.hal.sound(330 if s.mouth else 260, 15)
                if s.count == 0:
                    s.level += 1; s.score += 100; s.reset_maze(); s.reset_actors(o); s.freeze = o.now() + 800; o.hal.sound(1500, 300)
    def step_ghost(s, o, i):
        g = s.gh[i]; now = o.now()
        if g["st"] == 0:
            if now >= g["t"]: g["st"] = 1
            return
        if g["st"] == 1:
            if g["x"] < 11: g["x"] += 1
            elif g["x"] > 11: g["x"] -= 1
            elif g["y"] > 5: g["y"] -= 1
            if g["x"] == 11 and g["y"] <= 5: g.update(st=2, dx=-1 if i & 1 else 1, dy=0)
            return
        DIRS = [(0, -1), (0, 1), (-1, 0), (1, 0)]
        tx, ty = s.px, s.py
        if i == 1: tx, ty = s.px + s.pdx * 3, s.py + s.pdy * 3
        if i == 2 and abs(s.px - g["x"]) + abs(s.py - g["y"]) < 8: tx, ty = 1, 13
        rnd = now < s.scared or random.randrange(100) < 12
        opts = []
        for dx, dy in DIRS:
            if dx == -g["dx"] and dy == -g["dy"]: continue
            nx = (g["x"] + dx) % 23; ny = g["y"] + dy
            if s.free(nx, ny): opts.append(((nx - tx) ** 2 + (ny - ty) ** 2, dx, dy))
        if not opts: g["dx"], g["dy"] = -g["dx"], -g["dy"]; return
        ch = random.choice(opts) if rnd else min(opts)
        g["dx"], g["dy"] = ch[1], ch[2]; g["x"] = (g["x"] + g["dx"]) % 23; g["y"] += g["dy"]
    def sprite(s, g, x, y, rows):
        for j in range(4):
            for i in range(4):
                if rows[j] & (8 >> i): g.pixel(x + i, y + j)
    def pac_sprite(s, g, x, y, dx, dy, op):
        r = [6, 15, 15, 6]
        if op:
            if dx > 0: r[1] = r[2] = 14
            elif dx < 0: r[1] = r[2] = 7
            elif dy < 0: r[0] = 0
            elif dy > 0: r[3] = 0
        s.sprite(g, x, y, r)
    def draw(s, o):
        g = o.gfx; now = o.now(); g.clear(); blink = (now >> 8) & 1; T = 4
        for y in range(15):
            for x in range(23):
                c = s.tile(x, y); px = x * T; py = s.MY + y * T
                if c == "#":
                    if s.tile(x - 1, y) != "#": g.vline(px, py, T)
                    if s.tile(x + 1, y) != "#" and x < 22: g.vline(px + 3, py, T)
                    if s.tile(x, y - 1) != "#": g.hline(px, py, T)
                    if s.tile(x, y + 1) != "#": g.hline(px, py + 3, T)
                elif c == "-": g.hline(px, py + 1, T)
                if s.dots[y][x]:
                    if c == "o":
                        if blink: g.fill(px + 1, py + 1, 2, 2)
                    else: g.pixel(px + 1, py + 1)
        sc = now < s.scared; fl = sc and (s.scared - now < 1800) and ((now >> 7) & 1)
        for gh in s.gh:
            r = [6, 9, 15, 10]
            if sc and gh["st"] != 0:
                r = [6, 15, 15, 10]
                if not fl: r = [v & (5 if j & 1 else 10) for j, v in enumerate(r)]
            s.sprite(g, gh["x"] * T, s.MY + gh["y"] * T, r)
        s.pac_sprite(g, s.px * T, s.MY + s.py * T, s.pdx, s.pdy, s.mouth)
        P = s.PANX; g.vline(P - 3, 0, 64); g.text(P, 1, "SCORE"); g.text(P, 10, s.score); g.text(P, 22, "HI"); g.text(P, 31, s.hi)
        g.text(P, 43, "LV"); g.text(P + 14, 43, s.level)
        for i in range(s.lives): s.pac_sprite(g, P + i * 7, 55, 1, 0, False)
        if now < s.ready: g.fill(30, 38, 40, 9, 0); g.text(32, 39, "READY!")
    def frame(s, o):
        p = o.hal.pressed; h = o.hal.held; now = o.now()
        if s.over:
            s.draw(o); g = o.gfx; g.fill(4, 18, 88, 30, 0); g.rect(4, 18, 88, 30)
            g.text(19, 22, "GAME OVER"); g.text(8, 34, "A:NOVO B:SAIR")
            if "A" in p: s.init(o)
            if "B" in p: return False
            return True
        for src in (p, h):
            for k, v in (("UP", (0, -1)), ("DOWN", (0, 1)), ("LEFT", (-1, 0)), ("RIGHT", (1, 0))):
                if k in src: s.wx, s.wy = v; break
            else: continue
            break
        if now >= s.freeze and now >= s.ready:
            pm = 90 if s.level > 4 else 125 - s.level * 7
            gm = 230 if now < s.scared else (105 if s.level > 6 else 170 - s.level * 10)
            if now >= s.nextpac: s.nextpac = now + pm; s.step_pac(o); s.hits(o)
            if now >= s.nextgh:
                s.nextgh = now + gm
                for i in range(3): s.step_ghost(o, i)
                s.hits(o)
        s.draw(o); return True

# ======================================================================
#  DOOM (raycasting)
# ======================================================================
class Doom:
    name = "DOOM"; d1 = "SHOOTER 3D"; d2 = "MATE DEMONIOS"
    MAP = [0xFFFF, 0x8001, 0xBC3D, 0xA005, 0xA425, 0x8421, 0x8661, 0x8001,
           0x8001, 0x8661, 0x8421, 0xA425, 0xA005, 0xBC3D, 0x8001, 0xFFFF]
    SPAWN = [(14, 1), (14, 14), (1, 14), (8, 3), (8, 12), (14, 7)]
    VH = 56
    def wall(s, cx, cy):
        if cx < 0 or cy < 0 or cx >= 16 or cy >= 16: return True
        return bool(s.MAP[cy] & (1 << cx))
    def solid(s, x, y): return s.wall(int(x) >> 8, int(y) >> 8)
    def blocked(s, x, y):
        r = 44
        return s.solid(x - r, y - r) or s.solid(x + r, y - r) or s.solid(x - r, y + r) or s.solid(x + r, y + r)
    def init(s, o):
        s.px = 2 * 256 + 128; s.py = 7 * 256 + 128; s.pa = 0; s.hp = 100; s.kills = 0; s.score = 0; s.wave = 1
        s.over = False; s.hi = o.hal.hi("doom"); s.fire_ready = s.fire_until = s.hurt_until = 0; s.last = o.now()
        s.zb = [255] * 64; s.spawn(o)
    def spawn(s, o):
        n = min(s.wave + 1, 4); s.en = []
        for i in range(4):
            if i >= n: s.en.append({"x": 0, "y": 0, "hp": 0, "atk": 0}); continue
            k = (i * 2 + s.wave) % 6; ex = s.SPAWN[k][0] * 256 + 128; ey = s.SPAWN[k][1] * 256 + 128
            if abs(ex - s.px) + abs(ey - s.py) < 1024:
                k = (k + 3) % 6; ex = s.SPAWN[k][0] * 256 + 128; ey = s.SPAWN[k][1] * 256 + 128
            s.en.append({"x": ex, "y": ey, "hp": 3 if s.wave > 4 else 2, "atk": o.now() + 1500})
    def project(s, e):
        dx = e["x"] - s.px; dy = e["y"] - s.py; a = s.pa * 2 * math.pi / 1024; c = math.cos(a); sn = math.sin(a)
        return dx * c + dy * sn, dy * c - dx * sn
    def shoot(s, o):
        best = None; bd = 1e9; zc = min(s.zb[31], s.zb[32])
        for e in s.en:
            if e["hp"] <= 0: continue
            d, sd = s.project(e)
            if d > 48 and abs(sd) < 80 and d < bd and d / 32 < zc: bd = d; best = e
        if best:
            best["hp"] -= 1
            if best["hp"] <= 0: s.kills += 1; s.score += 100; o.hal.sound(160, 120)
            else: o.hal.sound(300, 40)
    def vl(s, g, x, y, ln, c):
        y = int(y); ln = int(ln)
        if y < 0: ln += y; y = 0
        if y + ln > s.VH: ln = s.VH - y
        if ln > 0: g.vline(x, y, ln, c)
    @staticmethod
    def shade(nc, x, y):
        if nc < 10: return True
        if nc < 24: return ((x + y) & 1) == 0
        if nc < 48: return not (x & 1) and not (y & 1)
        return not (x & 3) and not (y & 3)
    def render_walls(s, g):
        for col in range(64):
            off = (col - 32) * 21 / 8; ang = (s.pa + off) * 2 * math.pi / 1024
            stx = math.cos(ang) / 8; sty = math.sin(ang) / 8; rx = s.px / 256; ry = s.py / 256; n = 0
            while True:
                rx += stx; ry += sty; n += 1
                if n >= 96 or s.wall(int(math.floor(rx)), int(math.floor(ry))): break
            nc = max(1, int(n * math.cos(off * 2 * math.pi / 1024)))
            s.zb[col] = min(255, nc); h = min(s.VH, 400 // nc); y0 = (s.VH - h) // 2; x = col * 2
            for y in range(y0, y0 + h):
                if s.shade(nc, x, y): g.pixel(x, y)
                if s.shade(nc, x + 1, y): g.pixel(x + 1, y)
    def draw_enemy(s, g, e):
        d, sd = s.project(e)
        if d <= 48: return
        sx = int(64 + sd * 111 / d)
        if sx < -80 or sx > 208: return
        w = max(2, int(14200 / d)); h = max(3, int(10240 / d)); yb = 28 + int(6400 / d); y0 = yb - h; dz = int(d) >> 5
        x0 = sx - w // 2; x1 = sx + w // 2; headh = h * 3 // 8
        for x in range(x0 - 1, x1 + 2):
            if x < 0 or x >= 128 or s.zb[x >> 1] <= dz: continue
            s.vl(g, x, y0 - 1, h + 2, 0)
            if x < x0 or x > x1: continue
            if x0 + w // 5 <= x <= x1 - w // 5: s.vl(g, x, y0, h, 1)
            else: s.vl(g, x, y0 + headh, h - headh, 1)
        if h >= 8:
            es = max(1, w // 8); eh = max(1, h // 10); ey = y0 + h // 8
            for side in (-1, 1):
                ex = sx + side * (w // 6) - (es if side < 0 else 0)
                for i in range(es):
                    if 0 <= ex + i < 128 and s.zb[(ex + i) >> 1] > dz:
                        for j in range(eh): g.pixel(ex + i, ey + j, 0)
            for i in range(-(w // 8), w // 8 + 1):
                mx = sx + i
                if 0 <= mx < 128 and s.zb[mx >> 1] > dz: g.pixel(mx, y0 + headh - 1, 0)
    def frame(s, o):
        g = o.gfx; now = o.now(); dt = max(15, min(100, now - s.last)); s.last = now; p = o.hal.pressed; h = o.hal.held
        if s.over:
            g.fill(10, 14, 108, 34, 0); g.rect(10, 14, 108, 34); g.text(30, 19, "VOCE MORREU")
            g.text(22, 29, "SCORE"); g.text(58, 29, s.score); g.text(16, 39, "A:NOVO  B:SAIR")
            if "A" in p: s.init(o)
            if "B" in p: return False
            return True
        a = s.pa * 2 * math.pi / 1024; c = math.cos(a) * 127; sn = math.sin(a) * 127; spd = dt * 3 / 512
        mx = my = 0.0
        if "UP" in h: mx += c * spd; my += sn * spd
        if "DOWN" in h: mx -= c * spd; my -= sn * spd
        turn = dt * 15 // 16
        if "B" in h:
            if "RIGHT" in h: mx -= sn * spd; my += c * spd
            if "LEFT" in h: mx += sn * spd; my -= c * spd
        else:
            if "RIGHT" in h: s.pa = (s.pa + turn) % 1024
            if "LEFT" in h: s.pa = (s.pa - turn) % 1024
        if not s.blocked(s.px + mx, s.py): s.px += mx
        if not s.blocked(s.px, s.py + my): s.py += my
        if "A" in p and now >= s.fire_ready:
            s.fire_ready = now + 300; s.fire_until = now + 100; o.hal.sound(120, 60); s.shoot(o)
        alive = 0; sp = dt * 3 / 16
        for e in s.en:
            if e["hp"] <= 0: continue
            alive += 1; dx = s.px - e["x"]; dy = s.py - e["y"]; man = abs(dx) + abs(dy)
            if 330 < man < 2600:
                sx = max(-sp, min(sp, dx)); sy = max(-sp, min(sp, dy))
                if not s.blocked(e["x"] + sx, e["y"]): e["x"] += sx
                if not s.blocked(e["x"], e["y"] + sy): e["y"] += sy
            if man <= 400 and now >= e["atk"]:
                e["atk"] = now + 700; s.hp -= 10; s.hurt_until = now + 120; o.hal.sound(90, 120)
        if s.hp <= 0:
            s.hp = 0; s.over = True; o.hal.save_hi("doom", s.score); s.hi = o.hal.hi("doom"); o.hal.sound(100, 500)
        if not alive:
            s.wave += 1; s.score += 50; s.hp = min(100, s.hp + 20); s.spawn(o); o.hal.sound(1200, 200)
        g.clear()
        for y, st in ((32, 10), (36, 8), (41, 6), (47, 4), (54, 3)):
            for x in range(0, 128, st): g.pixel(x, y)
        s.render_walls(g)
        for e in sorted((e for e in s.en if e["hp"] > 0), key=lambda e: -s.project(e)[0]): s.draw_enemy(g, e)
        fire = now < s.fire_until; off = 3 if fire else 0
        g.fill(58, 44 + off, 12, 12 - off, 0); g.fill(62, 44 + off, 4, 7); g.fill(60, 50 + off, 8, 6 - off); g.fill(63, 46 + off, 2, 3, 0)
        if fire: g.fill(61, 37, 6, 5); g.fill(63, 34, 2, 11); g.fill(58, 39, 12, 2)
        for px_, py_ in ((62, 28), (66, 28), (64, 26), (64, 30)): g.pixel(px_, py_, 2)
        g.fill(0, 56, 128, 8, 0); g.hline(0, 56, 128); g.fill(1, 58, 7, 6); g.pixel(2, 60, 0); g.pixel(6, 60, 0)
        if s.hp > 60: g.hline(3, 62, 3, 0)
        elif s.hp > 30: g.pixel(4, 62, 0)
        else: g.pixel(3, 62, 0); g.pixel(5, 62, 0)
        g.text(11, 57, "HP"); g.rect(25, 58, 42, 6); g.fill(26, 59, int(s.hp * 40 / 100), 4)
        g.text(72, 57, "W"); g.text(78, 57, s.wave); g.text(92, 57, "K"); g.text(98, 57, s.kills)
        if now < s.hurt_until: g.invert_view(7)
        return True

# ======================================================================
#  SYSTEM
# ======================================================================
class System:
    name = "SYSTEM"; d1 = "INFORMACOES"; d2 = "E RECORDES"
    def init(s, o): s.astart = 0; s.msg = 0; s.t0 = o.now()
    def frame(s, o):
        g = o.gfx; p = o.hal.pressed; h = o.hal.held; now = o.now()
        if "UP" in p: o.hal.sound_on = not o.hal.sound_on
        if "B" in p and "A" not in h: return False
        if "A" in h:
            if not s.astart: s.astart = now
            if now - s.astart > 2000: o.hal.reset(); s.msg = now + 1500; s.astart = 0
        else: s.astart = 0
        g.clear(); g.fill(0, 0, 128, 9); g.text(2, 1, "SYSTEM", 1, 0); g.text(70, 1, "B=VOLTAR", 1, 0)
        sec = int((now - s.t0) / 1000)
        g.text(0, 12, "ATMEGA328 16MHZ 5V"); g.text(0, 21, "FLASH 32K RAM 2K"); g.text(0, 30, "RAM LIVRE: ~900B (SIM)")
        g.text(0, 39, "UPTIME: %02d:%02d" % (sec // 60 % 100, sec % 60))
        g.text(0, 48, "D:%d P:%d T:%d" % (o.hal.hi("doom"), o.hal.hi("pacman"), o.hal.hi("tetris")))
        if now < s.msg: g.text(0, 57, "RECORDES ZERADOS!")
        else: g.text(0, 57, "UP:SOM LIGADO" if o.hal.sound_on else "UP:SOM MUDO"); g.text(86, 57, "A:ZERAR")
        return True

# ======================================================================
#  KERNEL (boot + launcher)
# ======================================================================
BOOT = ["GILIARDEOS V1.0 BOOT", "[ OK ] CPU 16MHZ 8BIT", "[ OK ] 2K RAM 32K ROM", "[ OK ] OLED 128X64",
        "[ OK ] BOTOES + SOM", "[ OK ] EEPROM 1K", "[ OK ] 4 APPS PRONTOS", "INICIANDO INTERFACE.."]

class GiliardeOS:
    def __init__(s, fake_clock=None):
        s.fake = fake_clock; s.t0 = time.monotonic()
        s.gfx = Gfx(); s.hal = Hal(s.now); s.apps = [Doom(), Pacman(), Tetris(), System()]
        s.running = None; s.sel = 0; s.boot_t0 = s.now(); s.combo = None; s.state = "boot"
    def now(s):
        return s.fake[0] if s.fake else int((time.monotonic() - s.t0) * 1000)
    def launch(s, i):
        s.running = s.apps[i]; s.running.init(s); s.combo = None; s.state = "app"; s.hal.sound(988, 60)
    def go_menu(s): s.running = None; s.state = "menu"; s.combo = None
    def icon(s, i, cx, cy):
        g = s.gfx
        if i == 0:
            g.circle(cx, cy - 3, 10); g.fill(cx - 6, cy + 4, 13, 7); g.circle(cx - 4, cy - 4, 3, 0); g.circle(cx + 4, cy - 4, 3, 0)
            g.fill(cx - 1, cy + 1, 3, 3, 0)
            for k in (-2, 0, 2): g.vline(cx + k, cy + 7, 4, 0)
        elif i == 1:
            g.circle(cx - 3, cy, 10)
            for dx in range(12): hh = dx * 3 // 5; g.vline(cx - 3 + dx, cy - hh, hh * 2 + 1, 0)
            g.fill(cx + 11, cy - 1, 2, 2); g.fill(cx + 16, cy - 1, 2, 2)
        elif i == 2:
            for bx, by in ((-10, -11), (-3, -11), (4, -11), (-3, -4), (-10, 3), (-3, 3), (4, 3), (4, 10)): g.fill(cx + bx, cy + by, 6, 6)
        else:
            g.circle(cx, cy, 9); g.fill(cx - 2, cy - 13, 4, 5); g.fill(cx - 2, cy + 9, 4, 5); g.fill(cx - 13, cy - 2, 5, 4); g.fill(cx + 9, cy - 2, 5, 4)
            for dx, dy in ((-10, -10), (8, -10), (-10, 8), (8, 8)): g.fill(cx + dx, cy + dy, 3, 3)
            g.circle(cx, cy, 3, 0)
    def boot_frame(s):
        g = s.gfx; t = s.now() - s.boot_t0; g.clear()
        starts = [0] + [500 + 260 * i for i in range(7)]       # instante em que cada linha aparece
        end_log = starts[-1] + 260
        if t < end_log:
            for j, st in enumerate(starts):
                if t >= st: g.text(0, j * 8, BOOT[j])
            return
        t2 = t - end_log
        pr = min(100, int(t2 / 700 * 100)); g.rect(0, 0, 128, 64); g.text(4, 14, "GILIARDEOS", 2); g.hline(4, 31, 118)
        g.text(25, 36, "SISTEMA 8-BIT"); g.text(13, 46, "FEIRA DE CIENCIAS"); g.rect(14, 56, 100, 5); g.fill(14, 56, pr, 5)
        if t2 > 1100: s.state = "menu"
    def menu_frame(s):
        p = s.hal.pressed; n = len(s.apps)
        if "RIGHT" in p or "DOWN" in p: s.sel = (s.sel + 1) % n; s.hal.sound(700, 20)
        if "LEFT" in p or "UP" in p: s.sel = (s.sel - 1) % n; s.hal.sound(700, 20)
        if "A" in p: s.launch(s.sel); return
        g = s.gfx; a = s.apps[s.sel]; g.clear(); g.fill(0, 0, 128, 9); g.text(2, 1, "GILIARDEOS", 1, 0); g.text(104, 1, "V1.0", 1, 0)
        g.rect(3, 12, 42, 40); s.icon(s.sel, 24, 32); g.text(50, 15, a.name, 2); g.hline(50, 31, 76)
        g.text(50, 36, a.d1); g.text(50, 45, a.d2); g.text(2, 56, "A=ABRIR")
        for i in range(n):
            if i == s.sel: g.fill(70 + i * 9, 57, 5, 5)
            else: g.rect(70 + i * 9, 57, 5, 5)
        if (s.now() >> 8) & 1: g.text(110, 56, "<"); g.text(120, 56, ">")
    def frame(s):
        s.hal.update()
        if s.state == "boot": s.boot_frame()
        elif s.state == "menu": s.menu_frame()
        else:
            h = s.hal.held; both = "A" in h and "B" in h; now = s.now()
            if both:
                if s.combo is None: s.combo = now
            else: s.combo = None
            alive = s.running.frame(s)
            if s.combo is not None:
                t = now - s.combo; s.gfx.fill(0, 62, int(t * 128 / 600), 2, 2)
                if t >= 600: alive = False
            if not alive: s.go_menu(); s.hal.sound(500, 60)

# ======================================================================
#  saida: terminal (ASCII) e janela (tkinter)
# ======================================================================
def ascii_frame(fb):
    out = []
    for y in range(0, H, 2):
        row = ""
        for x in range(W):
            t = fb[y * W + x]; b = fb[(y + 1) * W + x]
            row += "█" if t and b else "▀" if t else "▄" if b else " "
        out.append(row)
    return "\n".join(out)

HELP = """comandos:
  help              esta ajuda
  info              hardware simulado e estado atual
  run doom|pacman|tetris|system     abre um app direto
  menu              volta ao menu
  boot              reinicia o sistema (mostra o boot de novo)
  scores            mostra os recordes salvos
  reset             zera os recordes
  sound on|off      liga/desliga o som (indicador na janela)
  ascii             desenha a tela atual aqui no terminal
  save              salva a tela atual em screenshot.txt
  press <botao>     aperta um botao (up down left right a b) por 1 quadro
  exit              fecha o simulador"""

def run_window():
    import tkinter as tk
    os_ = GiliardeOS(); cmds = queue.Queue(); SC = 5
    root = tk.Tk(); root.title("GiliardeOS - OLED 0.96\" 128x64 (simulador)"); root.configure(bg="#111")
    img = tk.PhotoImage(width=W, height=H); lab = tk.Label(root, bd=6, bg="#222"); lab.pack()
    info = tk.Label(root, text="Setas = direcional | Z = A | X = B | Esc = sair do app", bg="#111", fg="#aaa", font=("Consolas", 9)); info.pack()
    ON_Y, ON_B, OFF = "#ffd21a", "#35c8ff", "#05070a"       # topo amarelo, resto azul (igual ao modulo real)
    keymap = {"Up": "UP", "Down": "DOWN", "Left": "LEFT", "Right": "RIGHT", "z": "A", "Z": "A", "x": "B", "X": "B"}
    pulse = []
    def kd(e):
        if e.keysym in keymap: os_.hal.held.add(keymap[e.keysym])
        elif e.keysym == "Escape" and os_.state == "app": os_.go_menu()
    def ku(e):
        if e.keysym in keymap: os_.hal.held.discard(keymap[e.keysym])
    root.bind("<KeyPress>", kd); root.bind("<KeyRelease>", ku)

    def handle(cmd):
        a = cmd.strip().lower().split()
        if not a: return
        c = a[0]
        if c == "help": print(HELP)
        elif c == "info":
            print("GiliardeOS v1.0 | ATmega328 16MHz | 2KB RAM | 32KB flash | OLED 128x64 | estado:", os_.state,
                  "| app:", os_.running.name if os_.running else "-", "| som:", "on" if os_.hal.sound_on else "off")
        elif c == "run" and len(a) > 1:
            for i, ap in enumerate(os_.apps):
                if ap.name.lower() == a[1]: os_.launch(i); print("abrindo", ap.name); break
            else: print("app desconhecido. use: doom pacman tetris system")
        elif c == "menu": os_.go_menu()
        elif c == "boot": os_.state = "boot"; os_.boot_t0 = os_.now(); os_.running = None
        elif c == "scores": print({k: os_.hal.hi(k) for k in ("doom", "pacman", "tetris")})
        elif c == "reset": os_.hal.reset(); print("recordes zerados")
        elif c == "sound" and len(a) > 1: os_.hal.sound_on = a[1] == "on"; print("som", a[1])
        elif c == "ascii": print(ascii_frame(os_.gfx.fb))
        elif c == "save": open("screenshot.txt", "w", encoding="utf-8").write(ascii_frame(os_.gfx.fb)); print("salvo em screenshot.txt")
        elif c == "press" and len(a) > 1 and a[1].upper() in BTNS: pulse.append(a[1].upper())
        elif c in ("exit", "quit"): root.destroy()
        else: print("comando invalido. digite 'help'")

    def shell():
        print("GiliardeOS shell - digite 'help'")
        while True:
            try: cmds.put(input("gos> "))
            except EOFError: break
    threading.Thread(target=shell, daemon=True).start()

    def tick():
        t0 = time.monotonic()
        try:
            while True: handle(cmds.get_nowait())
        except queue.Empty: pass
        injected = set(pulse); pulse.clear(); os_.hal.held |= injected
        os_.frame(); os_.hal.held -= injected
        fb = os_.gfx.fb; rows = []
        for y in range(H):
            on = ON_Y if y < 16 else ON_B; seg = fb[y * W:(y + 1) * W]
            rows.append("{" + " ".join(on if v else OFF for v in seg) + "}")
        img.put(" ".join(rows)); big = img.zoom(SC, SC); lab.configure(image=big); lab.image = big
        root.title("GiliardeOS (simulador)" + ("  ♪" if os_.hal.last_beep and os_.hal.sound_on else ""))
        os_.hal.last_beep = None
        root.after(max(1, 30 - int((time.monotonic() - t0) * 1000)), tick)
    tick(); root.mainloop()

def self_test():
    clk = [0]; os_ = GiliardeOS(fake_clock=clk); random.seed(1); os_.hal.save = lambda: None
    def step(n, keys=()):
        for _ in range(n):
            clk[0] += 33; os_.hal.held = set(keys); os_.frame()
    step(120)                                           # boot completo
    assert os_.state == "menu", os_.state
    print("[ok] boot -> menu"); print(ascii_frame(os_.gfx.fb))
    for i, ap in enumerate(os_.apps):
        os_.launch(i); rnd = random.Random(i)
        for f in range(900):
            keys = {b for b in BTNS if rnd.random() < 0.18}
            if i == 0 and f % 7 == 0: keys.add("A")
            clk[0] += 33; os_.hal.held = keys; os_.frame()
            if os_.state != "app": os_.launch(i)
        print("[ok] %s: 900 quadros sem erro" % ap.name)
        if i == 2: print(ascii_frame(os_.gfx.fb))
    os_.go_menu(); os_.hal.held = set(); print("TESTE OK")

if __name__ == "__main__":
    if "--test" in sys.argv: self_test()
    else: run_window()
