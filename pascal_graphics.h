/*
 * pascal_graphics.h  —  Turbo Pascal Graph-unit compatible SDL2 backend
 *
 * Included by pascal_interpreter.cpp when HAVE_SDL2 is defined.
 * Implements these Pascal built-in calls (case-insensitive):
 *
 *   GRAPH LIFECYCLE
 *     initgraph(driver, mode, path)   — open 640x480 window
 *     closegraph                       — close window
 *     graphresult                      — 0 if ok
 *     grapherrormsg(code)              — error string
 *     cleardevice                      — clear to background colour
 *     delay(ms)                        — sleep N milliseconds
 *     readkey                          — wait for a keypress (returns char)
 *
 *   COLOUR
 *     setcolor(c)                      — set fg draw colour (0-15 or RGB int)
 *     setbkcolor(c)                    — set background colour
 *     getcolor                         — current fg colour
 *     getbkcolor                       — current bg colour
 *     setrgbcolor(r,g,b)               — set fg colour by RGB (extension)
 *     setrgbbkcolor(r,g,b)             — set bg colour by RGB (extension)
 *
 *   DRAWING
 *     putpixel(x, y, color)
 *     getpixel(x, y)                   — returns colour at pixel
 *     line(x1, y1, x2, y2)
 *     lineto(x, y)                     — line from current pos to (x,y)
 *     linerel(dx, dy)                  — line relative to current pos
 *     moveto(x, y)                     — move current position
 *     moverel(dx, dy)
 *     rectangle(x1, y1, x2, y2)       — outline rectangle
 *     bar(x1, y1, x2, y2)             — filled rectangle (no border)
 *     circle(x, y, r)
 *     arc(x, y, start, end, r)        — arc in degrees
 *     ellipse(x, y, start, end, rx, ry)
 *     fillellipse(x, y, rx, ry)
 *     floodfill(x, y, border)
 *     drawpoly(n, points)             — not implemented (noop)
 *
 *   FILL
 *     setfillstyle(pattern, color)    — pattern 1=solid; color sets fill colour
 *
 *   TEXT
 *     outtextxy(x, y, s)             — draw text at pixel position
 *     outtext(s)                     — draw at current position, advance CP
 *     settextstyle(font,dir,size)    — no-op (uses built-in font)
 *     settextjustify(h,v)            — no-op
 *     textwidth(s)                   — returns pixel width estimate
 *     textheight(s)                  — returns pixel height (8px per row)
 *
 *   QUERY
 *     getmaxx                        — screen width - 1
 *     getmaxy                        — screen height - 1
 *     getx                           — current position x
 *     gety                           — current position y
 */

#pragma once
#ifdef HAVE_SDL2
#include <unordered_set>

#include <SDL2/SDL.h>
#include <cmath>
#include <string>

// ── Turbo Pascal 16-colour palette ──────────────────────────────────────
static const SDL_Color TP_PALETTE[16] = {
    {  0,   0,   0, 255}, // 0  Black
    {  0,   0, 170, 255}, // 1  Blue
    {  0, 170,   0, 255}, // 2  Green
    {  0, 170, 170, 255}, // 3  Cyan
    {170,   0,   0, 255}, // 4  Red
    {170,   0, 170, 255}, // 5  Magenta
    {170,  85,   0, 255}, // 6  Brown
    {170, 170, 170, 255}, // 7  LightGray
    { 85,  85,  85, 255}, // 8  DarkGray
    { 85,  85, 255, 255}, // 9  LightBlue
    { 85, 255,  85, 255}, // 10 LightGreen
    { 85, 255, 255, 255}, // 11 LightCyan
    {255,  85,  85, 255}, // 12 LightRed
    {255,  85, 255, 255}, // 13 LightMagenta
    {255, 255,  85, 255}, // 14 Yellow
    {255, 255, 255, 255}, // 15 White
};

// ── Built-in 8×8 bitmap font (ASCII 32-127) ────────────────────────────
// Uses the classic 8×8 IBM PC font, stored 1 byte per row, 8 rows per char.
// Only printable ASCII (32-126); others show as space.
static const uint8_t FONT8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 space
    {0x18,0x18,0x18,0x18,0x00,0x18,0x00,0x00}, // 33 !
    {0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00}, // 34 "
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // 35 #
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00}, // 36 $
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, // 37 %
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // 38 &
    {0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00}, // 39 '
    {0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00}, // 40 (
    {0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00}, // 41 )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 .
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // 47 /
    {0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0x7C,0x00}, // 48 0
    {0x30,0x70,0x30,0x30,0x30,0x30,0xFC,0x00}, // 49 1
    {0x78,0xCC,0x0C,0x38,0x60,0xCC,0xFC,0x00}, // 50 2
    {0x78,0xCC,0x0C,0x38,0x0C,0xCC,0x78,0x00}, // 51 3
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, // 52 4
    {0xFC,0xC0,0xF8,0x0C,0x0C,0xCC,0x78,0x00}, // 53 5
    {0x38,0x60,0xC0,0xF8,0xCC,0xCC,0x78,0x00}, // 54 6
    {0xFC,0xCC,0x0C,0x18,0x30,0x30,0x30,0x00}, // 55 7
    {0x78,0xCC,0xCC,0x78,0xCC,0xCC,0x78,0x00}, // 56 8
    {0x78,0xCC,0xCC,0x7C,0x0C,0x18,0x70,0x00}, // 57 9
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // 58 :
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // 59 ;
    {0x18,0x30,0x60,0xC0,0x60,0x30,0x18,0x00}, // 60 <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 =
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // 62 >
    {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, // 63 ?
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00}, // 64 @
    {0x30,0x78,0xCC,0xCC,0xFC,0xCC,0xCC,0x00}, // 65 A
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, // 66 B
    {0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, // 67 C
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, // 68 D
    {0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, // 69 E
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, // 70 F
    {0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00}, // 71 G
    {0xCC,0xCC,0xCC,0xFC,0xCC,0xCC,0xCC,0x00}, // 72 H
    {0x78,0x30,0x30,0x30,0x30,0x30,0x78,0x00}, // 73 I
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, // 74 J
    {0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, // 75 K
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, // 76 L
    {0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00}, // 77 M
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, // 78 N
    {0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x38,0x00}, // 79 O
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, // 80 P
    {0x78,0xCC,0xCC,0xCC,0xDC,0x78,0x1C,0x00}, // 81 Q
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, // 82 R
    {0x78,0xCC,0xE0,0x70,0x1C,0xCC,0x78,0x00}, // 83 S
    {0xFC,0xB4,0x30,0x30,0x30,0x30,0x78,0x00}, // 84 T
    {0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xFC,0x00}, // 85 U
    {0xCC,0xCC,0xCC,0xCC,0xCC,0x78,0x30,0x00}, // 86 V
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00}, // 87 W
    {0xC6,0xC6,0x6C,0x38,0x38,0x6C,0xC6,0x00}, // 88 X
    {0xCC,0xCC,0xCC,0x78,0x30,0x30,0x78,0x00}, // 89 Y
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, // 90 Z
    {0x78,0x60,0x60,0x60,0x60,0x60,0x78,0x00}, // 91 [
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // 92 backslash
    {0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00}, // 93 ]
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, // 94 ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00}, // 95 _
    {0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00}, // 96 `
    {0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, // 97 a
    {0xE0,0x60,0x60,0x7C,0x66,0x66,0xDC,0x00}, // 98 b
    {0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00}, // 99 c
    {0x1C,0x0C,0x0C,0x7C,0xCC,0xCC,0x76,0x00}, // 100 d
    {0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00}, // 101 e
    {0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00}, // 102 f
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, // 103 g
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, // 104 h
    {0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00}, // 105 i
    {0x0C,0x00,0x0C,0x0C,0x0C,0xCC,0xCC,0x78}, // 106 j
    {0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, // 107 k
    {0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00}, // 108 l
    {0x00,0x00,0xCC,0xFE,0xFE,0xD6,0xC6,0x00}, // 109 m
    {0x00,0x00,0xF8,0xCC,0xCC,0xCC,0xCC,0x00}, // 110 n
    {0x00,0x00,0x78,0xCC,0xCC,0xCC,0x78,0x00}, // 111 o
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, // 112 p
    {0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, // 113 q
    {0x00,0x00,0xDC,0x76,0x66,0x60,0xF0,0x00}, // 114 r
    {0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00}, // 115 s
    {0x10,0x30,0x7C,0x30,0x30,0x34,0x18,0x00}, // 116 t
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, // 117 u
    {0x00,0x00,0xCC,0xCC,0xCC,0x78,0x30,0x00}, // 118 v
    {0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00}, // 119 w
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, // 120 x
    {0x00,0x00,0xCC,0xCC,0xCC,0x7C,0x0C,0xF8}, // 121 y
    {0x00,0x00,0xFC,0x98,0x30,0x64,0xFC,0x00}, // 122 z
    {0x1C,0x30,0x30,0xE0,0x30,0x30,0x1C,0x00}, // 123 {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 |
    {0xE0,0x30,0x30,0x1C,0x30,0x30,0xE0,0x00}, // 125 }
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // 126 ~
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 127 DEL
};

// ── Graphics state ───────────────────────────────────────────────────────
struct GfxState {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture*  texture  = nullptr;  // pixel backbuffer
    std::vector<uint32_t> pixels;      // ARGB8888

    int width  = 640;
    int height = 480;
    int curX   = 0, curY = 0;         // current position

    SDL_Color fgColor = {255,255,255,255};  // white
    SDL_Color bgColor = {0,  0,  0,  255};  // black
    SDL_Color fillColor = {255,255,255,255};

    int graphResult = 0;  // 0 = grOk
    bool open = false;

    uint32_t toARGB(SDL_Color c) const {
        return (uint32_t(c.a) << 24) | (uint32_t(c.r) << 16) |
               (uint32_t(c.g) << 8)  |  uint32_t(c.b);
    }

    SDL_Color fromPalette(long long idx) const {
        if (idx >= 0 && idx < 16) return TP_PALETTE[idx];
        // Allow raw 24-bit RGB packed as integer
        SDL_Color c;
        c.r = (idx >> 16) & 0xFF;
        c.g = (idx >>  8) & 0xFF;
        c.b =  idx        & 0xFF;
        c.a = 255;
        return c;
    }

    void setPixel(int x, int y, SDL_Color c) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        pixels[y * width + x] = toARGB(c);
    }

    uint32_t getPixelRaw(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        return pixels[y * width + x];
    }

    void flush() {
        if (!texture) return;
        SDL_UpdateTexture(texture, nullptr, pixels.data(), width * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    void drawChar(int x, int y, char ch, SDL_Color color) {
        int idx = (unsigned char)ch - 32;
        if (idx < 0 || idx >= 96) idx = 0;
        const uint8_t* glyph = FONT8x8[idx];
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col))
                    setPixel(x + col, y + row, color);
            }
        }
    }

    void drawText(int x, int y, const std::string& s, SDL_Color color) {
        int cx = x;
        for (char c : s) {
            drawChar(cx, y, c, color);
            cx += 8;
        }
    }

    // Bresenham line
    void drawLine(int x1, int y1, int x2, int y2, SDL_Color c) {
        int dx = std::abs(x2-x1), sx = x1 < x2 ? 1 : -1;
        int dy = -std::abs(y2-y1), sy = y1 < y2 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            setPixel(x1, y1, c);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }

    // Midpoint circle — outline only
    void drawCircle(int cx, int cy, int r, SDL_Color c) {
        int x = 0, y = r, d = 1 - r;
        auto plot8 = [&](int px, int py) {
            setPixel(cx+px, cy+py, c); setPixel(cx-px, cy+py, c);
            setPixel(cx+px, cy-py, c); setPixel(cx-px, cy-py, c);
            setPixel(cx+py, cy+px, c); setPixel(cx-py, cy+px, c);
            setPixel(cx+py, cy-px, c); setPixel(cx-py, cy-px, c);
        };
        while (x <= y) {
            plot8(x, y);
            if (d < 0) d += 2*x+3; else { d += 2*(x-y)+5; y--; }
            x++;
        }
    }

    // Filled ellipse using horizontal scanlines
    void fillEllipse(int cx, int cy, int rx, int ry, SDL_Color c) {
        for (int y = -ry; y <= ry; y++) {
            double t = (double)y / ry;
            int hw = (int)(rx * std::sqrt(std::max(0.0, 1.0 - t*t)));
            for (int x = -hw; x <= hw; x++)
                setPixel(cx+x, cy+y, c);
        }
    }

    // Arc (degrees, counterclockwise from +X axis like TP)
    void drawArc(int cx, int cy, int startDeg, int endDeg, int r, SDL_Color c) {
        if (endDeg < startDeg) endDeg += 360;
        int steps = std::max(r * 4, 36);
        double prev_x = -1e9, prev_y = -1e9;
        for (int i = 0; i <= steps; i++) {
            double angle = M_PI * (startDeg + (endDeg - startDeg) * i / (double)steps) / 180.0;
            double px = cx + r * std::cos(angle);
            double py = cy - r * std::sin(angle);  // y flipped (screen coords)
            if (prev_x > -1e8)
                drawLine((int)prev_x, (int)prev_y, (int)px, (int)py, c);
            prev_x = px; prev_y = py;
        }
    }

    // Scanline flood fill
    void floodFill(int x, int y, SDL_Color borderColor) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        uint32_t borderRaw = toARGB(borderColor);
        uint32_t fillRaw   = toARGB(fillColor);
        uint32_t targetRaw = getPixelRaw(x, y);
        if (targetRaw == borderRaw || targetRaw == fillRaw) return;

        std::vector<std::pair<int,int>> stack;
        stack.push_back({x, y});
        while (!stack.empty()) {
            auto [cx, cy] = stack.back(); stack.pop_back();
            if (cx < 0 || cy < 0 || cx >= width || cy >= height) continue;
            uint32_t p = getPixelRaw(cx, cy);
            if (p == borderRaw || p == fillRaw) continue;
            pixels[cy * width + cx] = fillRaw;
            stack.push_back({cx+1, cy});
            stack.push_back({cx-1, cy});
            stack.push_back({cx, cy+1});
            stack.push_back({cx, cy-1});
        }
    }
};

static GfxState gfx;

// ── SDL2 event pump (call periodically so window doesn't freeze) ─────────
static void pumpEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            gfx.flush();
        }
    }
}

// ── Graphics built-in dispatch ────────────────────────────────────────────
// Called from Interpreter::evalCall when name matches a graph procedure.
// Returns {true, Value} if handled; {false, {}} otherwise.

// Names that are ONLY ever graphics calls regardless of window state
static bool isAlwaysGraphicsName(const std::string& name) {
    static const std::unordered_set<std::string> always = {
        "initgraph","closegraph","graphresult","grapherrormsg","cleardevice",
        "putpixel","getpixel","lineto","linerel","moveto","moverel",
        "rectangle","fillellipse","floodfill","drawpoly",
        "setcolor","setbkcolor","getcolor","getbkcolor",
        "setrgbcolor","setrgbbkcolor","setfillstyle",
        "outtextxy","outtext","settextstyle","settextjustify",
        "textwidth","textheight","getmaxx","getmaxy","getx","gety",
        "detectgraph",
    };
    return always.count(name) > 0;
}

// Names that shadow common Pascal identifiers — only intercept when window is open
static bool isOpenOnlyGraphicsName(const std::string& name) {
    static const std::unordered_set<std::string> openonly = {
        "bar","arc","line","circle","ellipse","delay","readkey",
    };
    return openonly.count(name) > 0;
}

static std::pair<bool, Value> evalGraphCall(
    const std::string& name,
    const std::vector<ASTPtr>& arg_nodes,
    std::function<Value(const ASTPtr&)> eval)
{
    // Only intercept if this is a graphics name AND (it's always-graphics OR window is open)
    bool isGraph = isAlwaysGraphicsName(name) ||
                   (isOpenOnlyGraphicsName(name) && gfx.open);
    if (!isGraph) return {false, Value{}};

    auto iarg = [&](int i) -> long long { return eval(arg_nodes[i]).as_int(); };
    auto rarg = [&](int i) -> double    { return eval(arg_nodes[i]).as_real(); };
    auto sarg = [&](int i) -> std::string { return eval(arg_nodes[i]).to_string(); };

    // ── Lifecycle ────────────────────────────────────────────────────────
    if (name == "initgraph") {
        if (gfx.open) return {true, Value{}};
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            gfx.graphResult = -1;
            return {true, Value{}};
        }
        gfx.window = SDL_CreateWindow("Pascal Graphics",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            gfx.width, gfx.height, SDL_WINDOW_SHOWN);
        if (!gfx.window) { gfx.graphResult = -2; return {true, Value{}}; }
        gfx.renderer = SDL_CreateRenderer(gfx.window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        gfx.texture = SDL_CreateTexture(gfx.renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
            gfx.width, gfx.height);
        gfx.pixels.assign(gfx.width * gfx.height, gfx.toARGB(gfx.bgColor));
        gfx.open = true;
        gfx.graphResult = 0;
        // Pump events and flush several times so macOS shows the window
        // (macOS requires the event loop to run before windows appear)
        for (int i = 0; i < 10; i++) {
            pumpEvents();
            gfx.flush();
            SDL_Delay(10);
        }
        return {true, Value{}};
    }

    if (name == "closegraph") {
        if (!gfx.open) return {true, Value{}};
        // Final flush to make sure all pixels are shown
        pumpEvents();
        gfx.flush();
        pumpEvents();
        SDL_Delay(200);
        SDL_DestroyTexture(gfx.texture);
        SDL_DestroyRenderer(gfx.renderer);
        SDL_DestroyWindow(gfx.window);
        SDL_Quit();
        gfx = GfxState{};
        return {true, Value{}};
    }

    if (name == "graphresult") {
        int r = gfx.graphResult;
        gfx.graphResult = 0;  // resets after read, like TP
        return {true, Value::from_int(r)};
    }

    if (name == "grapherrormsg") {
        long long code = iarg(0);
        std::string msg = (code == 0) ? "No error" : "Graphics error " + std::to_string(code);
        return {true, Value::from_string(msg)};
    }

    if (name == "cleardevice") {
        if (!gfx.open) return {true, Value{}};
        uint32_t bg = gfx.toARGB(gfx.bgColor);
        std::fill(gfx.pixels.begin(), gfx.pixels.end(), bg);
        gfx.curX = gfx.curY = 0;
        pumpEvents();
        gfx.flush();
        pumpEvents();
        return {true, Value{}};
    }

    if (name == "delay") {
        SDL_Delay((uint32_t)iarg(0));
        pumpEvents();
        return {true, Value{}};
    }

    if (name == "readkey") {
        // Wait for a key event, pumping SDL in the meantime
        if (gfx.open) gfx.flush();
        SDL_Event e;
        while (true) {
            if (SDL_WaitEvent(&e)) {
                if (e.type == SDL_KEYDOWN) {
                    // Map SDL keycode to ASCII
                    char c = '\0';
                    SDL_Keycode k = e.key.keysym.sym;
                    if (k >= 32 && k < 127) c = (char)k;
                    else if (k == SDLK_RETURN) c = '\r';
                    else if (k == SDLK_ESCAPE) c = 27;
                    else if (k == SDLK_BACKSPACE) c = 8;
                    return {true, Value::from_char(c)};
                }
                if (e.type == SDL_QUIT) return {true, Value::from_char(27)};
            }
        }
    }

    // ── Colour ───────────────────────────────────────────────────────────
    if (name == "setcolor") {
        gfx.fgColor = gfx.fromPalette(iarg(0));
        return {true, Value{}};
    }
    if (name == "setbkcolor") {
        gfx.bgColor = gfx.fromPalette(iarg(0));
        return {true, Value{}};
    }
    if (name == "getcolor") {
        // Return as palette index 0-15 (best match) or packed RGB
        for (int i = 0; i < 16; i++) {
            auto& p = TP_PALETTE[i];
            if (p.r == gfx.fgColor.r && p.g == gfx.fgColor.g && p.b == gfx.fgColor.b)
                return {true, Value::from_int(i)};
        }
        return {true, Value::from_int(
            (gfx.fgColor.r << 16) | (gfx.fgColor.g << 8) | gfx.fgColor.b)};
    }
    if (name == "getbkcolor") {
        for (int i = 0; i < 16; i++) {
            auto& p = TP_PALETTE[i];
            if (p.r == gfx.bgColor.r && p.g == gfx.bgColor.g && p.b == gfx.bgColor.b)
                return {true, Value::from_int(i)};
        }
        return {true, Value::from_int(
            (gfx.bgColor.r << 16) | (gfx.bgColor.g << 8) | gfx.bgColor.b)};
    }
    if (name == "setrgbcolor") {
        gfx.fgColor = {(uint8_t)iarg(0), (uint8_t)iarg(1), (uint8_t)iarg(2), 255};
        return {true, Value{}};
    }
    if (name == "setrgbbkcolor") {
        gfx.bgColor = {(uint8_t)iarg(0), (uint8_t)iarg(1), (uint8_t)iarg(2), 255};
        return {true, Value{}};
    }

    // ── Drawing ──────────────────────────────────────────────────────────
    if (name == "putpixel") {
        int x = (int)iarg(0), y = (int)iarg(1);
        SDL_Color c = gfx.fromPalette(iarg(2));
        gfx.setPixel(x, y, c);
        // Don't flush every pixel — batch them, flushing every 1000 pixels
        // and pumping events every 5000 so the window stays responsive
        static int pixelCount = 0;
        pixelCount++;
        if (pixelCount % 1000 == 0) gfx.flush();
        if (pixelCount % 5000 == 0) pumpEvents();
        return {true, Value{}};
    }
    if (name == "getpixel") {
        int x = (int)iarg(0), y = (int)iarg(1);
        uint32_t p = gfx.getPixelRaw(x, y);
        long long packed = ((p >> 16) & 0xFF) << 16 | ((p >> 8) & 0xFF) << 8 | (p & 0xFF);
        // Try to return palette index
        SDL_Color c = {uint8_t((p>>16)&0xFF), uint8_t((p>>8)&0xFF), uint8_t(p&0xFF), 255};
        for (int i = 0; i < 16; i++) {
            if (TP_PALETTE[i].r == c.r && TP_PALETTE[i].g == c.g && TP_PALETTE[i].b == c.b)
                return {true, Value::from_int(i)};
        }
        return {true, Value::from_int(packed)};
    }
    if (name == "line") {
        int x1=(int)iarg(0),y1=(int)iarg(1),x2=(int)iarg(2),y2=(int)iarg(3);
        gfx.drawLine(x1,y1,x2,y2,gfx.fgColor);
        gfx.curX=x2; gfx.curY=y2;
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "lineto") {
        int x2=(int)iarg(0), y2=(int)iarg(1);
        gfx.drawLine(gfx.curX,gfx.curY,x2,y2,gfx.fgColor);
        gfx.curX=x2; gfx.curY=y2;
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "linerel") {
        int x2=gfx.curX+(int)iarg(0), y2=gfx.curY+(int)iarg(1);
        gfx.drawLine(gfx.curX,gfx.curY,x2,y2,gfx.fgColor);
        gfx.curX=x2; gfx.curY=y2;
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "moveto") {
        gfx.curX=(int)iarg(0); gfx.curY=(int)iarg(1);
        return {true, Value{}};
    }
    if (name == "moverel") {
        gfx.curX+=(int)iarg(0); gfx.curY+=(int)iarg(1);
        return {true, Value{}};
    }
    if (name == "rectangle") {
        int x1=(int)iarg(0),y1=(int)iarg(1),x2=(int)iarg(2),y2=(int)iarg(3);
        gfx.drawLine(x1,y1,x2,y1,gfx.fgColor);
        gfx.drawLine(x2,y1,x2,y2,gfx.fgColor);
        gfx.drawLine(x2,y2,x1,y2,gfx.fgColor);
        gfx.drawLine(x1,y2,x1,y1,gfx.fgColor);
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "bar") {
        int x1=(int)iarg(0),y1=(int)iarg(1),x2=(int)iarg(2),y2=(int)iarg(3);
        for (int y=y1; y<=y2; y++)
            for (int x=x1; x<=x2; x++)
                gfx.setPixel(x, y, gfx.fillColor);
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "circle") {
        gfx.drawCircle((int)iarg(0),(int)iarg(1),(int)iarg(2),gfx.fgColor);
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "arc") {
        gfx.drawArc((int)iarg(0),(int)iarg(1),(int)iarg(2),(int)iarg(3),(int)iarg(4),gfx.fgColor);
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "ellipse") {
        // ellipse(x,y,startAngle,endAngle,xRadius,yRadius)
        int cx=(int)iarg(0), cy=(int)iarg(1);
        int sa=(int)iarg(2), ea=(int)iarg(3);
        int rx=(int)iarg(4), ry=(int)iarg(5);
        if (ea < sa) ea += 360;
        int steps = std::max(rx+ry, 36) * 2;
        double px0=-1e9, py0=-1e9;
        for (int i=0; i<=steps; i++) {
            double a = M_PI*(sa+(ea-sa)*i/(double)steps)/180.0;
            double px=cx+rx*std::cos(a), py=cy-ry*std::sin(a);
            if (px0>-1e8) gfx.drawLine((int)px0,(int)py0,(int)px,(int)py,gfx.fgColor);
            px0=px; py0=py;
        }
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "fillellipse") {
        gfx.fillEllipse((int)iarg(0),(int)iarg(1),(int)iarg(2),(int)iarg(3),gfx.fillColor);
        // draw outline
        gfx.drawCircle((int)iarg(0),(int)iarg(1),
            ((int)iarg(2)+(int)iarg(3))/2, gfx.fgColor);
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "floodfill") {
        gfx.floodFill((int)iarg(0),(int)iarg(1),gfx.fromPalette(iarg(2)));
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "drawpoly") {
        // Stub — would need array access support
        return {true, Value{}};
    }

    // ── Fill style ───────────────────────────────────────────────────────
    if (name == "setfillstyle") {
        // pattern ignored (always solid); just set the fill colour
        gfx.fillColor = gfx.fromPalette(iarg(1));
        return {true, Value{}};
    }

    // ── Text output ──────────────────────────────────────────────────────
    if (name == "outtextxy") {
        int x=(int)iarg(0), y=(int)iarg(1);
        std::string s = sarg(2);
        gfx.drawText(x, y, s, gfx.fgColor);
        pumpEvents();
        gfx.flush();
        pumpEvents();
        return {true, Value{}};
    }
    if (name == "outtext") {
        std::string s = sarg(0);
        gfx.drawText(gfx.curX, gfx.curY, s, gfx.fgColor);
        gfx.curX += (int)s.size() * 8;
        gfx.flush();
        return {true, Value{}};
    }
    if (name == "settextstyle" || name == "settextjustify") {
        return {true, Value{}};  // no-op
    }
    if (name == "textwidth") {
        return {true, Value::from_int((long long)sarg(0).size() * 8)};
    }
    if (name == "textheight") {
        return {true, Value::from_int(8)};
    }

    // ── Query ────────────────────────────────────────────────────────────
    if (name == "getmaxx") return {true, Value::from_int(gfx.width  - 1)};
    if (name == "getmaxy") return {true, Value::from_int(gfx.height - 1)};
    if (name == "getx")    return {true, Value::from_int(gfx.curX)};
    if (name == "gety")    return {true, Value::from_int(gfx.curY)};

    return {false, Value{}};  // not a graphics call
}

#else
#include <unordered_set>
// SDL2 not available — stub that always returns "not handled"
static std::pair<bool, Value> evalGraphCall(
    const std::string&, const std::vector<ASTPtr>&,
    std::function<Value(const ASTPtr&)>)
{
    return {false, Value{}};
}
#endif // HAVE_SDL2
