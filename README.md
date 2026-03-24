# Pascal IDE — tvision-based

A Turbo Pascal 7-style IDE using the [tvision](https://github.com/magiblot/tvision) library.

## Requirements

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.14+
- [tvision](https://github.com/magiblot/tvision)
- The `pascal` interpreter binary (built separately)

---

## Build steps

### 1. Clone and build tvision

```bash
git clone https://github.com/magiblot/tvision
cd tvision
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd ..
```

### 2. Build the Pascal interpreter

```bash
g++ -std=c++17 -O2 -o pascal pascal_interpreter.cpp
```

### 3. Build the IDE

Put `pascal_ide.cpp`, `CMakeLists.txt`, and `pascal_interpreter.cpp` in the same directory.

```bash
# From the IDE source directory:
cmake -S . -B build -DTVISION_DIR=../tvision -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

If your tvision clone is somewhere else, set `TVISION_DIR` accordingly:
```bash
cmake -S . -B build -DTVISION_DIR=/path/to/tvision
```

### 4. Run

Copy (or symlink) the `pascal` interpreter next to `pascal_ide`:
```bash
cp pascal build/pascal
./build/pascal_ide
# or open a file directly:
./build/pascal_ide myprogram.pas
```

---

## Keyboard shortcuts

| Key        | Action               |
|------------|----------------------|
| F2         | Save file            |
| F3         | Open file            |
| F9         | Run program          |
| F10        | Open menu bar        |
| Alt-X      | Exit                 |
| Alt-Bksp   | Undo                 |
| Shift-Del  | Cut                  |
| Ctrl-Ins   | Copy                 |
| Shift-Ins  | Paste                |
| Ctrl-L     | Find (TEditor built-in) |
| Ctrl-F     | Find & Replace (TEditor built-in) |

The editor is a full `TFileEditor` (the same one used in `tvedit`) so all standard Turbo Vision editor shortcuts work.

---

## Notes

- The `pascal` interpreter binary must be in the same directory as `pascal_ide`, or on your `PATH`.
- The IDE saves the file before running. If it hasn't been saved yet you'll be prompted for a filename.
- Programs that read from stdin will receive EOF immediately (stdin is redirected from `/dev/null` during runs).
