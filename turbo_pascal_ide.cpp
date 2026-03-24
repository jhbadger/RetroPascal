/*
 * Turbo Pascal IDE — A Turbo Pascal 7.0-style text UI for the Pascal interpreter
 *
 * Features:
 *   - Blue/cyan color scheme authentic to Turbo Pascal
 *   - Menu bar: File | Edit | Run | Help
 *   - Full-screen editor with syntax highlighting
 *   - Run output shown in bottom panel / full-screen output window
 *   - Status bar at bottom
 *   - File open/save dialogs
 *   - Keyboard shortcuts (F1-F10, Alt+key, Ctrl+key)
 *   - Line/column display
 *   - Error display
 *
 * Build:
 *   g++ -std=c++17 -O2 -o tpide turbo_pascal_ide.cpp -lpthread
 *
 * Requires a VT100-compatible terminal (any modern terminal emulator).
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <functional>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <signal.h>

// ═══════════════════════════════════════════
//  ANSI Terminal Control
// ═══════════════════════════════════════════
namespace Term {
    static termios orig_termios;
    static bool raw_mode_active = false;

    void enableRaw() {
        if (raw_mode_active) return;
        tcgetattr(STDIN_FILENO, &orig_termios);
        termios raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |=  (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        raw_mode_active = true;
    }

    void disableRaw() {
        if (!raw_mode_active) return;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = false;
    }

    struct Size { int rows, cols; };
    Size getSize() {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
            return {ws.ws_row, ws.ws_col};
        return {24, 80};
    }

    // Output helpers
    void write(const std::string& s) { ::write(STDOUT_FILENO, s.data(), s.size()); }
    void move(int row, int col)      { write("\033[" + std::to_string(row+1) + ";" + std::to_string(col+1) + "H"); }
    void clearScreen()               { write("\033[2J"); }
    void hideCursor()                { write("\033[?25l"); }
    void showCursor()                { write("\033[?25h"); }
    void saveCursor()                { write("\033[s"); }
    void restoreCursor()             { write("\033[u"); }
    void flush()                     { fflush(stdout); }

    // Colors: fg and bg are 0–7 ANSI color indices
    // Bold attribute for bright colors
    std::string color(int fg, int bg, bool bold = false) {
        return "\033[" + std::string(bold ? "1" : "0") +
               ";3" + std::to_string(fg) +
               ";4" + std::to_string(bg) + "m";
    }
    std::string reset() { return "\033[0m"; }

    // Color constants (ANSI)
    enum { BLACK=0, RED=1, GREEN=2, YELLOW=3, BLUE=4, MAGENTA=5, CYAN=6, WHITE=7 };

    // Turbo Pascal palette
    std::string tp_desktop()    { return color(CYAN,  BLUE,  false); } // dark blue desktop
    std::string tp_editor()     { return color(WHITE, BLUE,  false); } // white on blue editor
    std::string tp_menubar()    { return color(BLACK, CYAN,  false); } // black on cyan menubar
    std::string tp_menusel()    { return color(BLACK, WHITE, false); } // selected menu item
    std::string tp_menuhot()    { return color(RED,   CYAN,  true);  } // hotkey letter
    std::string tp_menuhot_sel(){ return color(RED,   WHITE, true);  } // hotkey in selected
    std::string tp_statusbar()  { return color(BLACK, CYAN,  false); } // status bar
    std::string tp_statuskey()  { return color(BLACK, WHITE, false); } // key hint
    std::string tp_titlebar()   { return color(BLACK, CYAN,  false); } // window title bar
    std::string tp_linenum()    { return color(CYAN,  BLUE,  false); } // line numbers
    std::string tp_dialog()     { return color(BLACK, CYAN,  false); } // dialog bg
    std::string tp_dialog_title(){ return color(BLACK, WHITE,false); } // dialog title
    std::string tp_dialog_btn() { return color(WHITE, BLUE,  false); } // dialog button
    std::string tp_dialog_btn_sel(){ return color(BLACK,WHITE,false); } // selected button
    std::string tp_output_bg()  { return color(BLACK, BLACK, false); } // output window
    std::string tp_output_fg()  { return color(WHITE, BLACK, false); } // output text
    std::string tp_error()      { return color(WHITE, RED,   true);  } // error text
    std::string tp_keyword()    { return color(WHITE, BLUE,  true);  } // bold white = keyword
    std::string tp_comment()    { return color(CYAN,  BLUE,  false); } // comment
    std::string tp_string_lit() { return color(YELLOW,BLUE,  false); } // string literal
    std::string tp_number()     { return color(CYAN,  BLUE,  true);  } // number
    std::string tp_cursor_line(){ return color(WHITE, CYAN,  false); } // current line highlight

    // Read a key (returns a string to handle escape sequences)
    std::string readKey() {
        char c;
        if (::read(STDIN_FILENO, &c, 1) != 1) return "";
        if (c == '\033') {
            char seq[8] = {};
            // Set non-blocking briefly
            int flags = fcntl(STDIN_FILENO, F_GETFL);
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
            int n = ::read(STDIN_FILENO, seq, sizeof(seq)-1);
            fcntl(STDIN_FILENO, F_SETFL, flags);
            if (n <= 0) return "ESC";
            std::string s(seq, n);
            if (s == "[A")  return "UP";
            if (s == "[B")  return "DOWN";
            if (s == "[C")  return "RIGHT";
            if (s == "[D")  return "LEFT";
            if (s == "[H")  return "HOME";
            if (s == "[F")  return "END";
            if (s == "[5~") return "PGUP";
            if (s == "[6~") return "PGDN";
            if (s == "[2~") return "INS";
            if (s == "[3~") return "DEL";
            if (s == "[1;5C") return "CTRL_RIGHT";
            if (s == "[1;5D") return "CTRL_LEFT";
            if (s == "[1;2A") return "SHIFT_UP";
            if (s == "[1;2B") return "SHIFT_DOWN";
            // F keys
            if (s == "OP" || s == "[11~") return "F1";
            if (s == "OQ" || s == "[12~") return "F2";
            if (s == "OR" || s == "[13~") return "F3";
            if (s == "OS" || s == "[14~") return "F4";
            if (s == "[15~") return "F5";
            if (s == "[17~") return "F6";
            if (s == "[18~") return "F7";
            if (s == "[19~") return "F8";
            if (s == "[20~") return "F9";
            if (s == "[21~") return "F10";
            // Alt keys
            if (s.size() == 1) return std::string("ALT_") + s;
            return "ESC";
        }
        if (c == 127 || c == 8) return "BACKSPACE";
        if (c == 13 || c == 10) return "ENTER";
        if (c == 9)  return "TAB";
        if (c == 1)  return "CTRL_A";
        if (c == 3)  return "CTRL_C";
        if (c == 4)  return "CTRL_D";
        if (c == 5)  return "CTRL_E";
        if (c == 6)  return "CTRL_F";
        if (c == 11) return "CTRL_K";
        if (c == 14) return "CTRL_N";
        if (c == 15) return "CTRL_O";
        if (c == 16) return "CTRL_P";
        if (c == 17) return "CTRL_Q";
        if (c == 18) return "CTRL_R";
        if (c == 19) return "CTRL_S";
        if (c == 24) return "CTRL_X";
        if (c == 25) return "CTRL_Y";
        if (c == 26) return "CTRL_Z";
        return std::string(1, c);
    }
}

// ═══════════════════════════════════════════
//  Pascal keywords for syntax highlighting
// ═══════════════════════════════════════════
static const std::vector<std::string> PASCAL_KEYWORDS = {
    "program","var","const","begin","end","if","then","else",
    "while","do","for","to","downto","repeat","until",
    "procedure","function","array","of","record","type",
    "div","mod","and","or","not","xor","in",
    "true","false","nil",
    "integer","real","boolean","string","char","byte","word","longint",
    "write","writeln","read","readln","halt","exit",
    "uses","unit","interface","implementation","forward"
};

// ═══════════════════════════════════════════
//  Syntax Highlighter
// ═══════════════════════════════════════════
struct Span {
    int start, len;
    enum Kind { NORMAL, KEYWORD, COMMENT, STRING, NUMBER, PREPROC } kind;
};

std::vector<Span> highlight(const std::string& line) {
    std::vector<Span> spans;
    int n = (int)line.size();
    int i = 0;
    while (i < n) {
        // Comment { }
        if (line[i] == '{') {
            int start = i++;
            while (i < n && line[i] != '}') i++;
            if (i < n) i++;
            spans.push_back({start, i-start, Span::COMMENT});
            continue;
        }
        // Comment //
        if (i+1 < n && line[i] == '/' && line[i+1] == '/') {
            spans.push_back({i, n-i, Span::COMMENT});
            i = n; continue;
        }
        // String
        if (line[i] == '\'') {
            int start = i++;
            while (i < n) {
                if (line[i] == '\'' && i+1 < n && line[i+1] == '\'') { i += 2; }
                else if (line[i] == '\'') { i++; break; }
                else i++;
            }
            spans.push_back({start, i-start, Span::STRING});
            continue;
        }
        // Number
        if (std::isdigit(line[i]) || (line[i] == '$' && i+1 < n && std::isxdigit(line[i+1]))) {
            int start = i++;
            while (i < n && (std::isalnum(line[i]) || line[i] == '.')) i++;
            spans.push_back({start, i-start, Span::NUMBER});
            continue;
        }
        // Identifier / keyword
        if (std::isalpha(line[i]) || line[i] == '_') {
            int start = i++;
            while (i < n && (std::isalnum(line[i]) || line[i] == '_')) i++;
            std::string word = line.substr(start, i-start);
            std::string lower = word;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            bool is_kw = std::find(PASCAL_KEYWORDS.begin(), PASCAL_KEYWORDS.end(), lower) != PASCAL_KEYWORDS.end();
            spans.push_back({start, i-start, is_kw ? Span::KEYWORD : Span::NORMAL});
            continue;
        }
        i++;
    }
    return spans;
}

// Render a line with syntax highlighting into an ANSI string, clipped to width
std::string renderLine(const std::string& line, int col_offset, int width, bool is_cursor_line) {
    std::string base_color = is_cursor_line ? Term::tp_cursor_line() : Term::tp_editor();
    auto spans = highlight(line);

    // Build color-per-char array
    std::vector<Span::Kind> char_kind(line.size(), Span::NORMAL);
    for (auto& sp : spans)
        for (int j = sp.start; j < sp.start + sp.len && j < (int)line.size(); j++)
            char_kind[j] = sp.kind;

    auto kindColor = [&](Span::Kind k) -> std::string {
        if (is_cursor_line) {
            // On cursor line, keep bg=CYAN
            switch(k) {
                case Span::KEYWORD: return "\033[1;37;46m"; // bold white on cyan
                case Span::COMMENT: return "\033[0;32;46m"; // green on cyan
                case Span::STRING:  return "\033[0;33;46m"; // yellow on cyan
                case Span::NUMBER:  return "\033[1;36;46m"; // bright cyan on cyan
                default:            return Term::tp_cursor_line();
            }
        }
        switch(k) {
            case Span::KEYWORD: return Term::tp_keyword();
            case Span::COMMENT: return Term::tp_comment();
            case Span::STRING:  return Term::tp_string_lit();
            case Span::NUMBER:  return Term::tp_number();
            default:            return Term::tp_editor();
        }
    };

    std::string out = base_color;
    int chars_written = 0;
    Span::Kind cur_kind = Span::NORMAL;

    for (int c = col_offset; c < (int)line.size() && chars_written < width; c++) {
        Span::Kind k = char_kind[c];
        if (k != cur_kind) { out += kindColor(k); cur_kind = k; }
        char ch = line[c];
        if (ch == '<') out += "<";
        else if (ch == '>') out += ">";
        else out += ch;
        chars_written++;
    }
    // Pad to width
    while (chars_written < width) { out += ' '; chars_written++; }
    return out;
}

// ═══════════════════════════════════════════
//  Editor Buffer
// ═══════════════════════════════════════════
class Editor {
public:
    std::vector<std::string> lines;
    int cursor_row = 0, cursor_col = 0;
    int scroll_row = 0, scroll_col = 0;
    std::string filename;
    bool modified = false;

    Editor() { lines.push_back(""); }

    void loadFile(const std::string& fname) {
        filename = fname;
        lines.clear();
        std::ifstream f(fname);
        if (f) {
            std::string l;
            while (std::getline(f, l)) {
                // Strip \r
                if (!l.empty() && l.back() == '\r') l.pop_back();
                lines.push_back(l);
            }
        }
        if (lines.empty()) lines.push_back("");
        cursor_row = cursor_col = scroll_row = scroll_col = 0;
        modified = false;
    }

    bool saveFile(const std::string& fname = "") {
        if (!fname.empty()) filename = fname;
        if (filename.empty()) return false;
        std::ofstream f(filename);
        if (!f) return false;
        for (auto& l : lines) f << l << "\n";
        modified = false;
        return true;
    }

    void insertChar(char c) {
        auto& l = lines[cursor_row];
        l.insert(cursor_col, 1, c);
        cursor_col++;
        modified = true;
    }

    void insertNewline() {
        auto& l = lines[cursor_row];
        // Auto-indent: copy leading whitespace
        std::string indent;
        for (char c : l) { if (c == ' ' || c == '\t') indent += c; else break; }
        std::string rest = l.substr(cursor_col);
        l = l.substr(0, cursor_col);
        lines.insert(lines.begin() + cursor_row + 1, indent + rest);
        cursor_row++;
        cursor_col = (int)indent.size();
        modified = true;
    }

    void backspace() {
        if (cursor_col > 0) {
            lines[cursor_row].erase(cursor_col-1, 1);
            cursor_col--;
            modified = true;
        } else if (cursor_row > 0) {
            cursor_col = (int)lines[cursor_row-1].size();
            lines[cursor_row-1] += lines[cursor_row];
            lines.erase(lines.begin() + cursor_row);
            cursor_row--;
            modified = true;
        }
    }

    void deleteChar() {
        auto& l = lines[cursor_row];
        if (cursor_col < (int)l.size()) {
            l.erase(cursor_col, 1);
            modified = true;
        } else if (cursor_row + 1 < (int)lines.size()) {
            l += lines[cursor_row+1];
            lines.erase(lines.begin() + cursor_row + 1);
            modified = true;
        }
    }

    void deleteLine() {
        if (lines.size() == 1) { lines[0] = ""; cursor_col = 0; modified = true; return; }
        lines.erase(lines.begin() + cursor_row);
        if (cursor_row >= (int)lines.size()) cursor_row = (int)lines.size()-1;
        cursor_col = std::min(cursor_col, (int)lines[cursor_row].size());
        modified = true;
    }

    void insertTab() {
        // Insert 2 spaces
        lines[cursor_row].insert(cursor_col, "  ");
        cursor_col += 2;
        modified = true;
    }

    void moveUp()    { if (cursor_row > 0) cursor_row--; clampCol(); }
    void moveDown()  { if (cursor_row < (int)lines.size()-1) cursor_row++; clampCol(); }
    void moveLeft()  { if (cursor_col > 0) cursor_col--; else if (cursor_row > 0) { cursor_row--; cursor_col = (int)lines[cursor_row].size(); } }
    void moveRight() { if (cursor_col < (int)lines[cursor_row].size()) cursor_col++; else if (cursor_row < (int)lines.size()-1) { cursor_row++; cursor_col = 0; } }
    void moveHome()  { cursor_col = 0; }
    void moveEnd()   { cursor_col = (int)lines[cursor_row].size(); }
    void pageUp(int page_size)   { cursor_row = std::max(0, cursor_row - page_size); clampCol(); }
    void pageDown(int page_size) { cursor_row = std::min((int)lines.size()-1, cursor_row + page_size); clampCol(); }
    void wordLeft() {
        if (cursor_col == 0) { moveLeft(); return; }
        auto& l = lines[cursor_row];
        int c = cursor_col - 1;
        while (c > 0 && !std::isalnum(l[c])) c--;
        while (c > 0 && std::isalnum(l[c-1])) c--;
        cursor_col = c;
    }
    void wordRight() {
        auto& l = lines[cursor_row];
        int c = cursor_col;
        while (c < (int)l.size() && !std::isalnum(l[c])) c++;
        while (c < (int)l.size() && std::isalnum(l[c])) c++;
        cursor_col = c;
    }

    void clampCol() {
        cursor_col = std::min(cursor_col, (int)lines[cursor_row].size());
    }

    void scroll(int edit_rows, int edit_cols) {
        if (cursor_row < scroll_row) scroll_row = cursor_row;
        if (cursor_row >= scroll_row + edit_rows) scroll_row = cursor_row - edit_rows + 1;
        if (cursor_col < scroll_col) scroll_col = cursor_col;
        if (cursor_col >= scroll_col + edit_cols - 5) scroll_col = cursor_col - edit_cols + 6;
    }

    int numLines() const { return (int)lines.size(); }
    const std::string& getLine(int r) const { return lines[r]; }
    std::string getText() const {
        std::string out;
        for (size_t i = 0; i < lines.size(); i++) {
            out += lines[i];
            if (i + 1 < lines.size()) out += "\n";
        }
        return out;
    }
};

// ═══════════════════════════════════════════
//  Embedded Pascal Interpreter (copied from pascal_interpreter.cpp)
//  — abbreviated include, we'll #include it as a header
// ═══════════════════════════════════════════

// Forward-declare the interpreter (we'll include the full logic inline below)
// For brevity, we call the OS to run the external `pascal` binary if present,
// or we compile and run inline. We'll use a pipe-based approach with fork/exec.

struct RunResult {
    std::string output;
    std::string error;
    bool success;
};

RunResult runPascal(const std::string& source) {
    // Write source to a temp file
    std::string tmpfile = "/tmp/tp_ide_run.pas";
    std::string outfile = "/tmp/tp_ide_out.txt";
    std::string errfile = "/tmp/tp_ide_err.txt";
    {
        std::ofstream f(tmpfile);
        if (!f) return {"", "Cannot write temp file", false};
        f << source;
    }

    // Try to find the pascal interpreter binary next to the IDE binary
    // Check common locations
    std::vector<std::string> candidates = {
        "./pascal",
        "/usr/local/bin/pascal",
        "/tmp/pascal",
    };
    std::string interp_path;
    for (auto& c : candidates) {
        if (access(c.c_str(), X_OK) == 0) { interp_path = c; break; }
    }

    RunResult res;

    if (interp_path.empty()) {
        // Inline interpreter — we'll embed the minimal interpreter directly
        res.error = "Pascal interpreter binary not found.\n"
                    "Place 'pascal' executable in the same directory as this IDE.";
        res.success = false;
        return res;
    }

    // Fork and exec
    pid_t pid = fork();
    if (pid < 0) {
        res.error = "fork() failed";
        res.success = false;
        return res;
    }
    if (pid == 0) {
        // Child
        int out_fd = open(outfile.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);
        int err_fd = open(errfile.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);
        if (out_fd >= 0) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        if (err_fd >= 0) { dup2(err_fd, STDERR_FILENO); close(err_fd); }
        // Redirect stdin from /dev/null
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) { dup2(null_fd, STDIN_FILENO); close(null_fd); }
        execlp(interp_path.c_str(), interp_path.c_str(), tmpfile.c_str(), nullptr);
        _exit(127);
    }
    // Parent: wait with timeout
    int status = 0;
    int waited = 0;
    while (waited < 10000) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0) break;
        usleep(10000); waited += 10;
    }
    if (waited >= 10000) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        res.error = "Execution timed out (10s)";
        res.success = false;
        return res;
    }

    auto readFile = [](const std::string& path) {
        std::ifstream f(path);
        return std::string(std::istreambuf_iterator<char>(f), {});
    };
    res.output = readFile(outfile);
    res.error  = readFile(errfile);
    res.success = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return res;
}

// ═══════════════════════════════════════════
//  Screen Renderer
// ═══════════════════════════════════════════


// Helper: repeat a UTF-8 string N times
static std::string strRepeat(const std::string& s, int n) {
    std::string r; r.reserve(s.size() * n);
    for (int i = 0; i < n; i++) r += s;
    return r;
}

class IDE {
    Editor editor;
    Term::Size term_size;
    bool running = true;
    bool show_output = false;
    std::string last_output;
    std::string last_error;
    bool last_run_success = false;
    int output_scroll = 0;
    std::string status_msg;
    std::string status_color;

    // Menu state
    bool menu_open = false;
    int  menu_active = 0;   // 0=File 1=Edit 2=Run 3=Help
    int  menu_item = 0;

    // Dialog state
    enum DialogType { NONE, OPEN_FILE, SAVE_AS, HELP, ABOUT, GOTO_LINE, CONFIRM_QUIT };
    DialogType dialog = NONE;
    std::string dialog_input;
    int dialog_btn = 0; // 0=OK 1=Cancel

    // Layout constants
    int menubar_row()   { return 0; }
    int editor_top()    { return 1; }
    int editor_bottom() { return show_output ? term_size.rows - 9 : term_size.rows - 2; }
    int editor_rows()   { return editor_bottom() - editor_top(); }
    int editor_cols()   { return term_size.cols - 6; } // leave room for line nums
    int output_top()    { return show_output ? editor_bottom() + 1 : term_size.rows; }
    int output_rows()   { return show_output ? term_size.rows - output_top() - 1 : 0; }
    int statusbar_row() { return term_size.rows - 1; }

    // ─── Menu definitions ───
    struct MenuItem { std::string label; std::string key; std::function<void()> action; };
    struct Menu { std::string title; int hot; std::vector<MenuItem> items; };

    std::vector<Menu> menus;

    void initMenus() {
        menus = {
            {"File", 0, {
                {"New",          "F2",     [&]{ newFile();      }},
                {"Open...",      "F3",     [&]{ openFileDialog(); }},
                {"Save",         "F2",     [&]{ saveFile();     }},
                {"Save As...",   "",       [&]{ saveAsDialog(); }},
                {"---",          "",       {}},
                {"Exit",         "Alt+X",  [&]{ confirmQuit();  }},
            }},
            {"Edit", 0, {
                {"Undo",         "Ctrl+Z", [&]{ /* TODO */ setStatus("Undo not implemented"); }},
                {"---",          "",       {}},
                {"Cut",          "Ctrl+X", [&]{ /* TODO */ setStatus("Cut not implemented"); }},
                {"Copy",         "Ctrl+C", [&]{ /* TODO */ setStatus("Copy not implemented"); }},
                {"Paste",        "Ctrl+V", [&]{ /* TODO */ setStatus("Paste not implemented"); }},
                {"---",          "",       {}},
                {"Go to Line...", "Ctrl+G", [&]{ gotoLineDialog(); }},
            }},
            {"Run", 0, {
                {"Run",          "F9",     [&]{ runProgram();   }},
                {"Parameters...", "",      [&]{ /* TODO */ setStatus("Parameters not implemented"); }},
            }},
            {"Help", 0, {
                {"Contents",     "F1",     [&]{ showHelp();     }},
                {"About...",     "",       [&]{ showAbout();    }},
            }},
        };
    }

    // ─── Status ───
    void setStatus(const std::string& msg, bool error = false) {
        status_msg = msg;
        status_color = error ? Term::tp_error() : Term::tp_statusbar();
    }

    // ─── Actions ───
    void newFile() {
        editor = Editor();
        editor.filename = "";
        setStatus("New file");
    }

    void openFileDialog() {
        dialog = OPEN_FILE;
        dialog_input = "";
        dialog_btn = 0;
    }

    void saveFile() {
        if (editor.filename.empty()) { saveAsDialog(); return; }
        if (editor.saveFile()) setStatus("Saved: " + editor.filename);
        else setStatus("Error saving file!", true);
    }

    void saveAsDialog() {
        dialog = SAVE_AS;
        dialog_input = editor.filename;
        dialog_btn = 0;
    }

    void gotoLineDialog() {
        dialog = GOTO_LINE;
        dialog_input = "";
        dialog_btn = 0;
    }

    void confirmQuit() {
        if (editor.modified) {
            dialog = CONFIRM_QUIT;
            dialog_btn = 0;
        } else {
            running = false;
        }
    }

    void runProgram() {
        show_output = true;
        last_output = "";
        last_error  = "";
        output_scroll = 0;
        // Save first
        if (editor.filename.empty()) editor.filename = "/tmp/tp_ide_unnamed.pas";
        editor.saveFile();

        setStatus("Running...");
        redraw();

        auto res = runPascal(editor.getText());
        last_output = res.output;
        last_error  = res.error;
        last_run_success = res.success;

        if (res.success)
            setStatus("Program completed successfully.");
        else
            setStatus("Run error: " + (res.error.empty() ? "unknown error" : res.error.substr(0, 60)), true);
    }

    void showHelp() {
        dialog = HELP;
        dialog_btn = 0;
    }

    void showAbout() {
        dialog = ABOUT;
        dialog_btn = 0;
    }

    // ─── Drawing ───
    std::string buf; // output buffer

    void bwrite(const std::string& s) { buf += s; }

    void drawMenuBar() {
        Term::move(menubar_row(), 0);
        bwrite(Term::tp_menubar());
        std::string bar(term_size.cols, ' ');
        bwrite(bar);
        Term::move(menubar_row(), 0);
        bwrite(Term::tp_menubar());
        bwrite(" ");

        for (int i = 0; i < (int)menus.size(); i++) {
            const auto& m = menus[i];
            bool active = menu_open && menu_active == i;
            if (active) bwrite(Term::tp_menusel());
            else bwrite(Term::tp_menubar());

            // Render hot key letter differently
            std::string title = m.title;
            int h = m.hot;
            bwrite(" ");
            for (int j = 0; j < (int)title.size(); j++) {
                if (j == h) {
                    if (active) bwrite(Term::tp_menuhot_sel());
                    else bwrite(Term::tp_menuhot());
                    bwrite(std::string(1, title[j]));
                    if (active) bwrite(Term::tp_menusel());
                    else bwrite(Term::tp_menubar());
                } else {
                    bwrite(std::string(1, title[j]));
                }
            }
            bwrite(" ");
            if (active) bwrite(Term::tp_menubar());
        }

        // Right side: filename
        std::string fname = editor.filename.empty() ? "Untitled" : editor.filename;
        if (editor.modified) fname += " *";
        // truncate if needed
        int max_fname = term_size.cols - 50;
        if ((int)fname.size() > max_fname && max_fname > 3)
            fname = "..." + fname.substr(fname.size() - max_fname + 3);
        int rpos = term_size.cols - (int)fname.size() - 1;
        if (rpos > 0) {
            Term::move(menubar_row(), rpos);
            bwrite(Term::tp_menubar());
            bwrite(fname);
        }
    }

    void drawDropMenu() {
        if (!menu_open) return;
        const auto& m = menus[menu_active];
        int menu_col = 1;
        for (int i = 0; i < menu_active; i++)
            menu_col += (int)menus[i].title.size() + 3;

        int menu_width = 22;
        for (auto& item : m.items)
            menu_width = std::max(menu_width, (int)item.label.size() + (int)item.key.size() + 4);

        int start_row = 1;

        // Shadow
        for (int r = 0; r < (int)m.items.size() + 2; r++) {
            if (start_row + r + 1 >= term_size.rows) break;
            Term::move(start_row + r + 1, menu_col + 1);
            bwrite("\033[0;30;40m"); // black
            for (int c = 0; c < menu_width + 1; c++) bwrite(" ");
        }

        // Border + items
        Term::move(start_row, menu_col);
        bwrite(Term::tp_dialog());
        bwrite("┌");
        for (int c = 0; c < menu_width; c++) bwrite("─");
        bwrite("┐");

        for (int i = 0; i < (int)m.items.size(); i++) {
            Term::move(start_row + 1 + i, menu_col);
            bool sel = (menu_item == i);
            if (sel) bwrite(Term::tp_menusel());
            else bwrite(Term::tp_dialog());
            bwrite("│");

            const auto& item = m.items[i];
            if (item.label == "---") {
                bwrite(Term::tp_dialog());
                for (int c = 0; c < menu_width; c++) bwrite("─");
            } else {
                std::string lbl = item.label;
                std::string key = item.key;
                int pad = menu_width - (int)lbl.size() - (int)key.size() - 1;
                bwrite(" ");
                bwrite(lbl);
                for (int p = 0; p <= pad; p++) bwrite(" ");
                bwrite(key);
            }
            if (sel) bwrite(Term::tp_menusel());
            else bwrite(Term::tp_dialog());
            bwrite("│");
        }

        Term::move(start_row + 1 + (int)m.items.size(), menu_col);
        bwrite(Term::tp_dialog());
        bwrite("└");
        for (int c = 0; c < menu_width; c++) bwrite("─");
        bwrite("┘");
    }

    void drawEditorWindow() {
        // Title bar
        Term::move(editor_top() - 0, 0);
        bwrite(Term::tp_titlebar());
        std::string title = "╔";
        int inner = term_size.cols - 2;
        title += strRepeat("═", inner/3);
        title += "╗";
        // Overwrite middle with filename
        std::string fname = " " + (editor.filename.empty() ? "Untitled" : editor.filename) + " ";
        int fstart = (inner - (int)fname.size()) / 2 + 1;
        bwrite(title.substr(0, fstart));
        bwrite(Term::color(Term::WHITE, Term::CYAN, true));
        bwrite(fname);
        bwrite(Term::tp_titlebar());
        bwrite(title.substr(fstart + fname.size()));

        int rows = editor_rows();
        int cols = editor_cols();
        editor.scroll(rows, cols);

        for (int r = 0; r < rows; r++) {
            int line_num = editor.scroll_row + r;
            Term::move(editor_top() + r, 0);

            // Line number gutter (5 chars)
            bwrite(Term::tp_linenum());
            if (line_num < editor.numLines()) {
                std::string num = std::to_string(line_num + 1);
                while ((int)num.size() < 4) num = " " + num;
                bwrite(num + " ");
            } else {
                bwrite("     ");
            }

            if (line_num < editor.numLines()) {
                bool is_cursor = (line_num == editor.cursor_row);
                bwrite(renderLine(editor.getLine(line_num), editor.scroll_col, cols, is_cursor));
            } else {
                bwrite(Term::tp_editor());
                std::string blank(cols, ' ');
                bwrite(blank);
            }

            // Right border
            bwrite(Term::tp_titlebar());
            bwrite("║");
        }

        // Bottom border
        Term::move(editor_top() + rows, 0);
        bwrite(Term::tp_titlebar());
        if (show_output) {
            bwrite("╠");
            bwrite(strRepeat("═", term_size.cols - 2));
            bwrite("╣");
            // Output panel title
            Term::move(editor_top() + rows, term_size.cols/2 - 8);
            bwrite(Term::color(Term::WHITE, Term::CYAN, true));
            bwrite(" Output/Messages ");
            bwrite(Term::tp_titlebar());
        } else {
            bwrite("╚");
            bwrite(strRepeat("═", term_size.cols - 2));
            bwrite("╝");
        }
    }

    void drawOutputPanel() {
        if (!show_output) return;
        int rows = output_rows();
        if (rows <= 0) return;

        // Split output into lines
        auto split = [](const std::string& s) {
            std::vector<std::string> lines;
            std::istringstream ss(s);
            std::string l;
            while (std::getline(ss, l)) lines.push_back(l);
            return lines;
        };

        auto out_lines = split(last_output);
        auto err_lines = split(last_error);

        // Combine: errors in red, output in white
        struct OutLine { std::string text; bool is_error; };
        std::vector<OutLine> all;
        for (auto& l : out_lines) all.push_back({l, false});
        if (!last_error.empty() && !last_run_success) {
            all.push_back({"", true});
            for (auto& l : err_lines) all.push_back({l, true});
        }

        int max_scroll = std::max(0, (int)all.size() - rows + 1);
        output_scroll = std::min(output_scroll, max_scroll);

        for (int r = 0; r < rows; r++) {
            Term::move(output_top() + r, 0);
            int idx = output_scroll + r;
            if (idx < (int)all.size()) {
                auto& ol = all[idx];
                if (ol.is_error) {
                    bwrite(Term::tp_error());
                    std::string line = ol.text;
                    while ((int)line.size() < term_size.cols) line += ' ';
                    if ((int)line.size() > term_size.cols) line = line.substr(0, term_size.cols);
                    bwrite(line);
                } else {
                    bwrite(Term::tp_output_fg());
                    bwrite(Term::tp_output_bg());
                    std::string line = ol.text;
                    while ((int)line.size() < term_size.cols) line += ' ';
                    if ((int)line.size() > term_size.cols) line = line.substr(0, term_size.cols);
                    bwrite(line);
                }
            } else {
                bwrite(Term::tp_output_bg());
                std::string blank(term_size.cols, ' ');
                bwrite(blank);
            }
        }

        // Close border
        Term::move(output_top() + rows, 0);
        bwrite(Term::tp_titlebar());
        bwrite("╚");
        bwrite(strRepeat("═", term_size.cols - 2));
        bwrite("╝");
    }

    void drawStatusBar() {
        Term::move(statusbar_row(), 0);
        bwrite(Term::tp_statusbar());
        std::string blank(term_size.cols, ' ');
        bwrite(blank);
        Term::move(statusbar_row(), 0);

        // Key hints
        auto hint = [&](const std::string& key, const std::string& label) {
            bwrite(Term::tp_statuskey());
            bwrite(key);
            bwrite(Term::tp_statusbar());
            bwrite(label);
            bwrite(" ");
        };
        hint("F1", "Help");
        hint("F2", "Save");
        hint("F3", "Open");
        hint("F9", "Run");
        hint("F10", "Menu");
        if (show_output) hint("F5", "Close Out");
        hint("Alt+X", "Exit");

        // Status message on right area
        if (!status_msg.empty()) {
            int msg_col = term_size.cols - (int)status_msg.size() - 2;
            if (msg_col > 40) {
                Term::move(statusbar_row(), msg_col);
                bwrite(status_color.empty() ? Term::tp_statusbar() : status_color);
                bwrite(status_msg);
                bwrite(Term::tp_statusbar());
            }
        }

        // Cursor position
        std::string pos = "Ln " + std::to_string(editor.cursor_row+1) +
                          " Col " + std::to_string(editor.cursor_col+1) +
                          " / " + std::to_string(editor.numLines());
        Term::move(statusbar_row(), term_size.cols - (int)pos.size() - 1);
        bwrite(Term::tp_statuskey());
        bwrite(pos);
    }

    // ─── Dialogs ───
    void drawCenteredDialog(int width, int height,
                            const std::string& title,
                            const std::vector<std::string>& body,
                            const std::vector<std::string>& buttons,
                            int sel_btn,
                            const std::string& input = "",
                            bool show_input = false) {
        int row = (term_size.rows - height) / 2;
        int col = (term_size.cols - width)  / 2;

        // Shadow
        for (int r = 1; r < height + 1; r++) {
            Term::move(row + r, col + 1);
            bwrite("\033[0;30;40m");
            for (int c = 0; c < width + 1; c++) bwrite(" ");
        }

        // Box
        Term::move(row, col);
        bwrite(Term::tp_dialog());
        bwrite("┌");
        for (int c = 0; c < width-2; c++) bwrite("─");
        bwrite("┐");

        // Title
        if (!title.empty()) {
            Term::move(row, col + (width - (int)title.size()) / 2 - 1);
            bwrite(Term::tp_dialog_title());
            bwrite(" " + title + " ");
        }

        for (int r = 1; r < height-1; r++) {
            Term::move(row + r, col);
            bwrite(Term::tp_dialog());
            bwrite("│");
            std::string blank(width-2, ' ');
            if (r-1 < (int)body.size()) {
                std::string line = body[r-1];
                // Center
                int pad = (width-2 - (int)line.size()) / 2;
                blank = std::string(std::max(0,pad), ' ') + line;
                while ((int)blank.size() < width-2) blank += ' ';
                blank = blank.substr(0, width-2);
            }
            bwrite(blank);
            bwrite("│");
        }

        // Input field
        if (show_input) {
            int input_row = row + height - 3;
            Term::move(input_row, col);
            bwrite(Term::tp_dialog());
            bwrite("│");
            bwrite(Term::color(Term::BLACK, Term::WHITE, false));
            std::string inp = input;
            int inp_width = width - 4;
            if ((int)inp.size() > inp_width) inp = inp.substr(inp.size() - inp_width);
            while ((int)inp.size() < inp_width) inp += ' ';
            bwrite(" " + inp + " ");
            bwrite(Term::tp_dialog());
            bwrite("│");
        }

        // Buttons
        Term::move(row + height - 2, col);
        bwrite(Term::tp_dialog());
        bwrite("│");
        int btn_area = width - 2;
        int btn_total = 0;
        for (auto& b : buttons) btn_total += (int)b.size() + 4;
        int btn_pad = (btn_area - btn_total) / ((int)buttons.size() + 1);
        bwrite(std::string(std::max(1, btn_pad), ' '));
        for (int i = 0; i < (int)buttons.size(); i++) {
            if (i == sel_btn) bwrite(Term::tp_dialog_btn_sel());
            else bwrite(Term::tp_dialog_btn());
            bwrite("[ " + buttons[i] + " ]");
            bwrite(Term::tp_dialog());
            if (i + 1 < (int)buttons.size())
                bwrite(std::string(std::max(1, btn_pad), ' '));
        }
        bwrite(Term::tp_dialog());
        bwrite("│");

        // Bottom border
        Term::move(row + height - 1, col);
        bwrite(Term::tp_dialog());
        bwrite("└");
        for (int c = 0; c < width-2; c++) bwrite("─");
        bwrite("┘");
    }

    void drawDialog() {
        if (dialog == NONE) return;

        if (dialog == OPEN_FILE) {
            std::vector<std::string> body = {
                "Enter filename to open:",
                "",
                "",
            };
            drawCenteredDialog(50, 7, " Open File ", body, {"OK", "Cancel"}, dialog_btn, dialog_input, true);
            // Place cursor in input field
            int row = (term_size.rows - 7) / 2;
            int col = (term_size.cols - 50) / 2;
            int input_row = row + 7 - 3;
            int inp_width = 50 - 4;
            std::string inp = dialog_input;
            int cpos = (int)inp.size();
            if (cpos > inp_width) { inp = inp.substr(inp.size() - inp_width); cpos = inp_width; }
            Term::move(input_row, col + 2 + cpos);
        }

        if (dialog == SAVE_AS) {
            std::vector<std::string> body = {
                "Save file as:",
                "",
                "",
            };
            drawCenteredDialog(50, 7, " Save As ", body, {"OK", "Cancel"}, dialog_btn, dialog_input, true);
        }

        if (dialog == GOTO_LINE) {
            std::vector<std::string> body = {
                "Go to line number:",
                "",
                "",
            };
            drawCenteredDialog(40, 7, " Go To Line ", body, {"OK", "Cancel"}, dialog_btn, dialog_input, true);
        }

        if (dialog == CONFIRM_QUIT) {
            drawCenteredDialog(44, 6, " Quit ",
                {"File has unsaved changes.", "Quit anyway?"}, {"Yes", "No"}, dialog_btn);
        }

        if (dialog == ABOUT) {
            drawCenteredDialog(52, 10, " About ",
                {"",
                 "Turbo Pascal IDE  v7.0",
                 "─────────────────────────────────",
                 "Pascal Interpreter + TUI",
                 "Built with C++17 + ANSI escape codes",
                 "",
                 "Press any key to close"}, {"OK"}, 0);
        }

        if (dialog == HELP) {
            drawCenteredDialog(58, 18, " Help ",
                {"F1         Help",
                 "F2         Save file",
                 "F3         Open file",
                 "F9         Compile and Run",
                 "F10        Open menu bar",
                 "F5         Toggle output panel",
                 "Alt+X      Exit",
                 "Ctrl+Y     Delete line",
                 "Ctrl+G     Go to line",
                 "Home/End   Start/end of line",
                 "PgUp/PgDn  Page up/down",
                 "Ctrl+←/→   Word left/right",
                 "Tab        Indent (2 spaces)",
                 "Arrow keys Navigation"}, {"OK"}, 0);
        }
    }

    // ─── Place cursor ───
    void placeCursor() {
        if (dialog != NONE || menu_open) return;
        int screen_row = editor_top() + (editor.cursor_row - editor.scroll_row);
        int screen_col = 5 + (editor.cursor_col - editor.scroll_col);
        if (screen_row >= editor_top() && screen_row < editor_top() + editor_rows() &&
            screen_col >= 5 && screen_col < term_size.cols - 1) {
            Term::move(screen_row, screen_col);
            Term::showCursor();
        } else {
            Term::hideCursor();
        }
    }

    // ─── Full redraw ───
    void redraw() {
        term_size = Term::getSize();
        buf.clear();
        Term::hideCursor();
        buf += "\033[?25l";

        drawMenuBar();
        drawEditorWindow();
        drawOutputPanel();
        drawStatusBar();

        if (menu_open) drawDropMenu();
        if (dialog != NONE) drawDialog();

        Term::write(buf);

        if (dialog == NONE && !menu_open) {
            placeCursor();
            if (dialog == NONE && !menu_open)
                Term::showCursor();
        }
        Term::flush();
    }

    // ─── Input handling ───
    void handleEditorKey(const std::string& key) {
        if (key == "UP")         editor.moveUp();
        else if (key == "DOWN")  editor.moveDown();
        else if (key == "LEFT")  editor.moveLeft();
        else if (key == "RIGHT") editor.moveRight();
        else if (key == "HOME")  editor.moveHome();
        else if (key == "END")   editor.moveEnd();
        else if (key == "PGUP")  editor.pageUp(editor_rows());
        else if (key == "PGDN")  editor.pageDown(editor_rows());
        else if (key == "CTRL_LEFT")  editor.wordLeft();
        else if (key == "CTRL_RIGHT") editor.wordRight();
        else if (key == "BACKSPACE")  editor.backspace();
        else if (key == "DEL")   editor.deleteChar();
        else if (key == "ENTER") editor.insertNewline();
        else if (key == "TAB")   editor.insertTab();
        else if (key == "CTRL_Y") editor.deleteLine();
        else if (key == "CTRL_G") gotoLineDialog();
        else if (key == "F1")  showHelp();
        else if (key == "F2")  saveFile();
        else if (key == "F3")  openFileDialog();
        else if (key == "F5")  { show_output = !show_output; }
        else if (key == "F9")  runProgram();
        else if (key == "F10") { menu_open = true; menu_active = 0; menu_item = 0; }
        else if (key == "ESC") { show_output = false; }
        else if (key == "ALT_x" || key == "ALT_X") confirmQuit();
        else if (key.size() == 1 && (unsigned char)key[0] >= 32) {
            editor.insertChar(key[0]);
            status_msg = "";
        }
    }

    void handleMenuKey(const std::string& key) {
        auto& m = menus[menu_active];
        if (key == "ESC" || key == "F10") {
            menu_open = false; return;
        }
        if (key == "LEFT") {
            menu_active = (menu_active - 1 + (int)menus.size()) % (int)menus.size();
            menu_item = 0;
        } else if (key == "RIGHT") {
            menu_active = (menu_active + 1) % (int)menus.size();
            menu_item = 0;
        } else if (key == "UP") {
            do { menu_item = (menu_item - 1 + (int)m.items.size()) % (int)m.items.size(); }
            while (m.items[menu_item].label == "---");
        } else if (key == "DOWN") {
            do { menu_item = (menu_item + 1) % (int)m.items.size(); }
            while (m.items[menu_item].label == "---");
        } else if (key == "ENTER") {
            auto action = m.items[menu_item].action;
            menu_open = false;
            if (action) action();
        }
    }

    void handleDialogKey(const std::string& key) {
        bool has_input = (dialog == OPEN_FILE || dialog == SAVE_AS || dialog == GOTO_LINE);

        if (key == "ESC") {
            dialog = NONE; return;
        }
        if (key == "TAB" || key == "LEFT" || key == "RIGHT") {
            int nbtns = (dialog == CONFIRM_QUIT) ? 2 : (dialog == ABOUT || dialog == HELP) ? 1 : 2;
            dialog_btn = (dialog_btn + 1) % nbtns;
            return;
        }
        if (key == "ENTER") {
            bool ok = (dialog_btn == 0);
            DialogType d = dialog;
            std::string inp = dialog_input;
            dialog = NONE;
            dialog_input = "";

            if (d == OPEN_FILE && ok) {
                if (!inp.empty()) {
                    editor.loadFile(inp);
                    setStatus("Opened: " + inp);
                }
            } else if (d == SAVE_AS && ok) {
                if (!inp.empty()) {
                    editor.saveFile(inp);
                    setStatus("Saved: " + inp);
                }
            } else if (d == GOTO_LINE && ok) {
                try {
                    int n = std::stoi(inp) - 1;
                    n = std::max(0, std::min(n, editor.numLines()-1));
                    editor.cursor_row = n;
                    editor.cursor_col = 0;
                } catch (...) {}
            } else if (d == CONFIRM_QUIT && ok) {
                running = false;
            } else if (d == ABOUT || d == HELP) {
                // closed
            }
            return;
        }

        if (!has_input) return;

        if (key == "BACKSPACE") {
            if (!dialog_input.empty()) dialog_input.pop_back();
        } else if (key.size() == 1 && (unsigned char)key[0] >= 32) {
            dialog_input += key;
        }
    }

public:
    IDE() {
        initMenus();
        // Start with a sample program
        editor.lines = {
            "program Hello;",
            "",
            "var",
            "  i, n: integer;",
            "  sum: integer;",
            "",
            "begin",
            "  writeln('Turbo Pascal IDE');",
            "  writeln('=================');",
            "",
            "  n := 10;",
            "  sum := 0;",
            "  for i := 1 to n do",
            "    sum := sum + i;",
            "",
            "  writeln('Sum 1..' + '10 = ', sum);",
            "  writeln;",
            "  writeln('Fibonacci sequence:');",
            "",
            "  var a, b, tmp: integer;",
            "  a := 0; b := 1;",
            "  for i := 1 to 15 do begin",
            "    write(a, ' ');",
            "    tmp := a + b;",
            "    a := b;",
            "    b := tmp",
            "  end;",
            "  writeln",
            "end.",
        };
        editor.modified = false;
        setStatus("Welcome to Turbo Pascal IDE  |  F1=Help  F9=Run  F10=Menu");
    }

    void run() {
        Term::enableRaw();
        Term::clearScreen();
        Term::hideCursor();

        // Hide terminal cursor initially
        Term::write("\033[?25l");

        // Attempt alternate screen (optional)
        Term::write("\033[?1049h");

        redraw();

        while (running) {
            std::string key = Term::readKey();
            if (key.empty()) continue;

            if (dialog != NONE) {
                handleDialogKey(key);
            } else if (menu_open) {
                handleMenuKey(key);
            } else {
                handleEditorKey(key);
            }

            // Handle output scroll in output panel
            if (show_output && !menu_open && dialog == NONE) {
                if (key == "CTRL_D") output_scroll++;
                if (key == "CTRL_U") output_scroll = std::max(0, output_scroll - 1);
            }

            redraw();
        }

        // Restore
        Term::write("\033[?1049l");
        Term::write("\033[?25h");
        Term::write("\033[0m");
        Term::disableRaw();
        std::cout << "\nGoodbye!\n";
    }
};

// ═══════════════════════════════════════════
//  main
// ═══════════════════════════════════════════
int main(int argc, char* argv[]) {
    // Handle SIGWINCH for terminal resize
    signal(SIGWINCH, [](int) { /* next redraw will pick up new size */ });

    IDE ide;

    // If a file was specified, load it
    if (argc >= 2) {
        // We need to access editor — pass via a method
        // (simplified: just note that loading happens in constructor-like flow)
        // For now, handle via the run loop initialization
        // We'll re-init after construction
    }

    ide.run();
    return 0;
}
