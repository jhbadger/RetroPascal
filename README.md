# RetroPascal

A complete Turbo Pascal development environment for modern systems — interpreter, native compiler, SDL2 graphics, and a tvision-based IDE that looks and feels like the original Borland Pascal IDE from 1992.

---

## What's included

| Component | File | Description |
|-----------|------|-------------|
| Interpreter + Compiler | `pascal_interpreter.cpp` | Run Pascal programs directly, or compile to native binaries via gcc |
| Graphics module | `pascal_graphics.h` | Turbo Pascal `Graph` unit implemented in SDL2 |
| IDE | `pascal_ide.cpp` | tvision-based editor with syntax highlighting, find/replace, run & compile |
| Build system | `CMakeLists.txt` | Builds everything; detects SDL2 automatically |

---

## Features

### Interpreter
- Integers, reals, booleans, characters, strings
- 1D arrays, `var` parameters (pass by reference)
- Procedures and functions with recursion
- `for` / `while` / `repeat..until` / `if..then..else`
- All standard built-ins: `writeln`, `readln`, `sqrt`, `sin`, `cos`, `arctan`, `exp`, `ln`, `round`, `trunc`, `length`, `copy`, `concat`, `pos`, `random`, `randomize`, and more
- Clear error messages with line numbers

### Compiler
Translates Pascal source → C → native binary via `gcc -O2`. The speedup is dramatic:

```
Mandelbrot set (640×480, 32 iterations):
  Interpreted:  ~19 seconds
  Compiled:     ~17 milliseconds   (~1100× faster)
```

Both modes produce identical output. The compiler handles all the same language features as the interpreter.

### Graphics (SDL2)
Compatible with Turbo Pascal's `Graph` unit:

```pascal
program Stars;
var gd, gm, i, x, y: integer;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;

  randomize;
  setbkcolor(1);   { blue background }
  cleardevice;

  for i := 1 to 500 do begin
    x := random(640);
    y := random(480);
    setcolor(random(15) + 1);
    putpixel(x, y, getcolor)
  end;

  outtextxy(200, 220, 'Press any key...');
  readkey;
  closegraph
end.
```

Supported procedures: `initgraph`, `closegraph`, `cleardevice`, `putpixel`, `getpixel`, `line`, `lineto`, `linerel`, `moveto`, `moverel`, `rectangle`, `bar`, `circle`, `arc`, `ellipse`, `fillellipse`, `floodfill`, `setcolor`, `setbkcolor`, `getcolor`, `getbkcolor`, `setrgbcolor`, `setrgbbkcolor`, `setfillstyle`, `outtextxy`, `outtext`, `textwidth`, `textheight`, `getmaxx`, `getmaxy`, `getx`, `gety`, `delay`, `readkey`, `graphresult`, `grapherrormsg`

Graphics work in both the interpreter and compiled binaries. If SDL2 is not available, programs compile cleanly with no-op stubs.

### IDE
Built on [magiblot/tvision](https://github.com/magiblot/tvision) — a modern port of the Borland Turbo Vision framework.

- **Syntax highlighting** — keywords, comments, strings, numbers
- **Find / Replace** — Ctrl-F, Ctrl-H, F7 (search again)
- **Error highlighting** — failed runs highlight the offending line in red and jump the cursor to it
- **F9 Run** — interpret the current file (stdin/stdout work, so `readln` works)
- **F8 Compile & Run** — compile to a native binary and run it
- **F2 Save, F3 Open** — file management with browser dialogs
- **Go to Line** — jump to any line number

---

## Building

### Prerequisites

**macOS (Homebrew):**
```bash
brew install sdl2
```

**Linux / Termux:**
```bash
apt install libsdl2-dev   # Debian/Ubuntu
pkg install sdl2          # Termux
```

**tvision** (required for the IDE):
```bash
git clone https://github.com/magiblot/tvision tvision
```

### Compile

```bash
cmake -S . -B build -DTVISION_DIR=./tvision
cmake --build build -j$(nproc)
```

This produces two binaries in `build/`:
- `pascal` — the interpreter and compiler
- `pascal_ide` — the IDE (`pascal` is copied alongside it automatically)

SDL2 is detected automatically via `sdl2-config`, CMake `find_package`, and `pkg-config` in that order. If not found, everything still builds — graphics calls become no-ops.

---

## Usage

### Interpreter
```bash
./pascal program.pas
```

### Compiler
```bash
# Compile to a binary (same name, no extension)
./pascal --compile program.pas

# Compile to a specific output name
./pascal --compile program.pas -o myprogram

./myprogram
```

### IDE
```bash
./pascal_ide                  # start with an empty file
./pascal_ide program.pas      # open a specific file
```

**Keyboard shortcuts:**

| Key | Action |
|-----|--------|
| F2 | Save |
| F3 | Open |
| F8 | Compile & Run |
| F9 | Run (interpret) |
| F10 | Menu |
| Ctrl-F | Find |
| Ctrl-H | Find & Replace |
| F7 | Search Again |
| Alt-X | Exit |

---

## Example programs

### Fibonacci
```pascal
program Fibonacci;
function fib(n: integer): integer;
begin
  if n <= 1 then fib := n
  else fib := fib(n-1) + fib(n-2)
end;
var i: integer;
begin
  for i := 0 to 15 do
    writeln('fib(', i, ') = ', fib(i))
end.
```

### Mandelbrot set
```pascal
program Mandelbrot;
var gd, gm, px, py, iter: integer;
    x, y, x0, y0, xtemp: real;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;

  for px := 0 to getmaxx do
    for py := 0 to getmaxy do begin
      x0 := (px / getmaxx) * 3.5 - 2.5;
      y0 := (py / getmaxy) * 2.0 - 1.0;
      x := 0; y := 0; iter := 0;
      while (x*x + y*y <= 4.0) and (iter < 64) do begin
        xtemp := x*x - y*y + x0;
        y := 2*x*y + y0;
        x := xtemp;
        iter := iter + 1
      end;
      if iter < 64 then
        putpixel(px, py, (iter mod 15) + 1)
    end;

  outtextxy(10, 10, 'Press any key');
  readkey;
  closegraph
end.
```

Interpreted (immediate start, takes ~20 seconds to render):
```bash
./pascal mandelbrot.pas
```

Compiled (renders in milliseconds):
```bash
./pascal --compile mandelbrot.pas
./mandelbrot
```

### Bouncing ball
```pascal
program BouncingBall;
var gd, gm, x, y, dx, dy, r, i: integer;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;

  x := 320; y := 240; dx := 3; dy := 2; r := 20;

  for i := 1 to 500 do begin
    setfillstyle(1, 0); fillellipse(x, y, r, r);  { erase }
    x := x + dx; y := y + dy;
    if (x - r < 0) or (x + r > getmaxx) then dx := -dx;
    if (y - r < 0) or (y + r > getmaxy) then dy := -dy;
    setcolor(14); setfillstyle(1, 12); fillellipse(x, y, r, r);
    delay(16)
  end;

  readkey;
  closegraph
end.
```

### Colour wheel
```pascal
program ColourWheel;
var gd, gm, i: integer; angle: real; r, g, b: integer;
begin
  gd := 0; gm := 0;
  initgraph(gd, gm, '');
  if graphresult <> 0 then halt;
  cleardevice;
  for i := 0 to 359 do begin
    angle := i * 3.14159 / 180;
    r := round(127 + 127 * sin(angle));
    g := round(127 + 127 * sin(angle + 2.094));
    b := round(127 + 127 * sin(angle + 4.189));
    setrgbcolor(r, g, b);
    line(320, 240,
         320 + round(200 * cos(angle)),
         240 + round(200 * sin(angle)))
  end;
  readkey;
  closegraph
end.
```

---

## Language reference

### Types

| Pascal | Notes |
|--------|-------|
| `integer` | 64-bit signed (compiled to `long long`) |
| `real` | 64-bit float (compiled to `double`) |
| `boolean` | `true` / `false` |
| `char` | Single character |
| `string` | Variable-length string (1024-byte buffer when compiled) |
| `array[lo..hi] of T` | 1D arrays of any type |

### Built-in functions

| Category | Functions |
|----------|-----------|
| Math | `abs`, `sqr`, `sqrt`, `sin`, `cos`, `tan`, `arctan`, `arcsin`, `arccos`, `exp`, `ln`, `log`, `round`, `trunc`, `frac`, `int`, `pi`, `max`, `min`, `odd` |
| String | `length`, `copy`, `concat`, `pos`, `upcase`, `lowercase`, `str`, `val` |
| Conversion | `ord`, `chr`, `succ`, `pred` |
| I/O | `write`, `writeln`, `read`, `readln` |
| Other | `random`, `randomize`, `halt`, `exit`, `sizeof` |

---

## Architecture

```
pascal_interpreter.cpp
├── Lexer          tokenises Pascal source into tokens
├── Parser         builds an AST from tokens
├── Interpreter    walks the AST and executes directly
│   └── evalGraphCall() ──► pascal_graphics.h  (SDL2 backend)
└── Compiler       walks the same AST and emits C source
    ├── genExpr()  expression → C expression
    ├── genStmt()  statement → C statement(s)
    ├── genDecl()  declarations (tracks variable types for writeln dispatch)
    └── compile()  assembles preamble + inline SDL2 + user code
                   then invokes: gcc -O2 [sdl2 flags] -lm
```

The compiler embeds a complete SDL2 graphics implementation directly in the generated C — no separate runtime library is needed. SDL2 flags are discovered automatically via `sdl2-config`.

---

## Known limitations

- Arrays are 1D only (no multi-dimensional arrays)
- No `unit` / `uses` support (single-file programs only)
- No records (`record`) or pointers
- No `case` statement in the compiler (interpreter only)
- Strings in compiled programs use fixed 1024-byte buffers
- The compiler infers print format from static type analysis — complex expressions with mixed types may occasionally need explicit casting

---

## License

MIT
