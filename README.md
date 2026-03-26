# RetroPascal

A Pascal interpreter and compiler written in a single C++ file (~4200 lines).

## Features

### Language
- Full Pascal: integers, reals, booleans, chars, strings, arrays (1D and 2D), records, objects
- `type` sections with `record` and `object(Parent)` inheritance and virtual dispatch
- `case` statements, `repeat...until`, `while`, `for`, `if/else`
- `in [...]` set membership expressions
- `goto`/`label` — via exception mechanism (interpreter only)
- `#N` char literals (`#13`, `#27`, etc.)
- String functions: `insert`, `delete`, `str`, `pos`, `length`, `copy`, `concat`, `upcase`
- String character access: `s[i]` read and write
- `gettickcount`, `gotoxy`, `clrscr`, `keypressed`, `readkey` (raw terminal, ANSI)
- Arrow key decoding: ESC[A/B/C/D → `#1`/`#2`/`#3`/`#4`
- `randomize`, `random(n)`, `chr`, `ord`, `abs`, `sqr`, `sqrt`, `sin`, `cos`, etc.
- `uses`/`label` declarations parsed and ignored
- `Shortint`/`Longint`/`Word`/`Byte` type aliases
- Shebang support (`#!/usr/bin/env pascal`)

### Compiler (Pascal → C via gcc)
- ~1000× speedup over interpreted mode
- Nested functions lifted to top level with name mangling
- `var` parameters correctly passed as pointers
- 2D array indexing as flat arrays
- Array-of-string: `char[N][1024]` with `strncpy`
- SDL2 graphics only linked when program uses graphics functions (`initgraph`, etc.)
- Terminal programs compile cleanly with no SDL dependency
- `setvbuf` + `write()` syscalls for immediate output (no buffering)
- Raw terminal mode (`VMIN=1`, `VTIME=0`) set at startup

## Building

```bash
g++ -std=c++17 -O2 -o pascal pascal_interpreter.cpp
```

Or with CMake:
```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## Usage

```bash
# Interpret
./pascal program.pas

# Compile to native binary
./pascal --compile program.pas -o program
./program

# Read from stdin
./pascal < program.pas
```

## Included Programs

- **snake.pas** — Terminal snake game with arrow key support
- **sudoku.pas** — Sudoku puzzle with arrow key navigation

## Regression Tests

8 test programs verified interpreter/compiler produce identical output:
`bar`, `t2`, `t6`, `final_test`, `test_records_full`, `test_objects`, `shapes`, `pets`

## Platform Support

Tested on macOS (Apple Silicon + Homebrew SDL2) and Android (Termux).
