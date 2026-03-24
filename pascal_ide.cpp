/*
 * Pascal IDE — Turbo Pascal 7-style IDE built on magiblot/tvision
 *
 * Build:
 *   cmake -S . -B build -DTVISION_DIR=./tvision
 *   cmake --build build -j$(nproc)
 *   cp pascal build/
 *   ./build/pascal_ide [file.pas]
 */

#define Uses_TApplication
#define Uses_TButton
#define Uses_TDeskTop
#define Uses_TDialog
#define Uses_TEditor
#define Uses_TEvent
#define Uses_TFileDialog
#define Uses_TFileEditor
#define Uses_TFrame
#define Uses_TIndicator
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TLabel
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TProgram
#define Uses_TRect
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TSubMenu
#define Uses_TWindow
#define Uses_MsgBox
#define Uses_TTerminal
#define Uses_otstream
#define Uses_TDrawBuffer
#define Uses_TScroller
#define Uses_TScreen
#define Uses_TColorAttr
#include <tvision/tv.h>

#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// Custom commands — all above 200 to avoid clashing with tvision built-ins
const ushort
    cmNewFile     = 201,
    cmOpenFile    = 202,
    cmSaveFile    = 203,
    cmSaveFileAs  = 204,
    cmRunProgram  = 205,
    cmAbout       = 206,
    cmGotoLine    = 207;

// ── Run pascal interpreter ────────────────────────────────────────────────
// Suspends tvision, runs the interpreter in the foreground (so the program
// can use stdin/stdout freely), then resumes tvision.
static void runPascalInteractive(const char* filepath) {
    // TProgram::dosShell() suspends tvision and restores the terminal.
    // We replicate what it does: suspend screen, fork+exec, wait, resume.
    TScreen::suspend();

    // Print a separator so the user knows the program is starting
    printf("\n--- Running %s ---\n\n", filepath);
    fflush(stdout);

    // execlp searches PATH automatically, so just use the binary name.
    // We also check for a copy next to the IDE binary first (common install layout).
    // We find the IDE's own directory via /proc/self/exe, then try "pascal" there,
    // then fall back to PATH.
    std::string interp = "pascal"; // default: search PATH
    {
        char self[4096] = {};
        ssize_t len = readlink("/proc/self/exe", self, sizeof(self)-1);
        if (len > 0) {
            std::string selfPath(self, len);
            std::string dir = selfPath.substr(0, selfPath.rfind('/') + 1);
            std::string candidate = dir + "pascal";
            if (access(candidate.c_str(), X_OK) == 0)
                interp = candidate;
        }
    }

    {
        pid_t pid = fork();
        if (pid == 0) {
            // Child: inherit stdin/stdout/stderr from the terminal.
            // execlp searches PATH if interp has no '/' in it.
            execlp(interp.c_str(), interp.c_str(), filepath, nullptr);
            perror("execlp: pascal not found");
            _exit(127);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                printf("\n[Process exited with code %d]\n", WEXITSTATUS(status));
            else if (!WIFEXITED(status))
                printf("\n[Process terminated abnormally]\n");
        } else {
            perror("fork");
        }
    }

    printf("\n--- Press Enter to return to the IDE ---");
    fflush(stdout);
    // Wait for Enter
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}

    TScreen::resume();
    // Force a full redraw
    TProgram::application->redraw();
}

// ── Output dialog ─────────────────────────────────────────────────────────
class TOutputDialog : public TDialog {
public:
    TOutputDialog(const std::string& text, bool success)
        : TWindowInit(&TOutputDialog::initFrame),
          TDialog(TRect(2, 1, 78, 22),
                  success ? " Program Output " : " Output / Errors ")
    {
        options |= ofCentered;
        TScrollBar* vbar = new TScrollBar(TRect(size.x-2, 1,        size.x-1, size.y-3));
        TScrollBar* hbar = new TScrollBar(TRect(1,        size.y-3, size.x-2, size.y-2));
        insert(vbar);
        insert(hbar);
        TRect r(1, 1, size.x-2, size.y-3);
        auto* term = new TTerminal(r, hbar, vbar, 32768);
        insert(term);
        otstream os(term);
        os << text;
        if (text.empty() || text.back() != '\n') os << '\n';
        os.flush();
        insert(new TButton(TRect((size.x-10)/2, size.y-2, (size.x+10)/2, size.y-1),
                           "  ~O~K  ", cmOK, bfDefault));
        selectNext(False);
    }
};


// ── Pascal syntax-highlighting editor ────────────────────────────────────
//
// Colour palette indices (passed to mapColor):
//   1 = normal text     (white on blue)
//   2 = selected text   (black on cyan)  -- tvision built-in selected
//   3 = keyword         (bold yellow on blue)
//   4 = comment         (cyan on blue)
//   5 = string/char     (bright green on blue)
//   6 = number          (bright cyan on blue)
//   7 = preprocessor/directive (magenta on blue)
//
// formatLine is called once per visible line; we walk the raw buffer,
// tokenise on the fly, and call TScreenCell::moveChar with the right
// colour index for each character.

static const char* PASCAL_KEYWORDS[] = {
    "program","unit","uses","interface","implementation","initialization","finalization",
    "var","const","type","label","procedure","function","begin","end","forward",
    "if","then","else","case","of","for","to","downto","do","while","repeat","until",
    "with","goto","exit","halt","break","continue",
    "and","or","not","xor","div","mod","shl","shr","in","is","as",
    "array","record","set","file","object","class","string","packed",
    "nil","true","false",
    "integer","real","boolean","char","byte","word","longint","shortint",
    "cardinal","int64","single","double","extended","comp","currency",
    "pointer","pchar","ansistring","widestring","shortstring",
    "write","writeln","read","readln","assigned","new","dispose","sizeof","typeof",
    nullptr
};

static bool isPascalKeyword(const char* p, int len) {
    char buf[32];
    if (len <= 0 || len >= 32) return false;
    for (int i = 0; i < len; i++) buf[i] = tolower((unsigned char)p[i]);
    buf[len] = '\0';
    for (int i = 0; PASCAL_KEYWORDS[i]; i++)
        if (strcmp(buf, PASCAL_KEYWORDS[i]) == 0) return true;
    return false;
}

// Token types
enum PasToken { ptNormal=1, ptKeyword=3, ptComment=4, ptString=5, ptNumber=6 };

class TPascalEditor : public TFileEditor {
public:
    TPascalEditor(const TRect& bounds,
                  TScrollBar* hScrollBar,
                  TScrollBar* vScrollBar,
                  TIndicator* indicator,
                  TStringView filename)
        : TFileEditor(bounds, hScrollBar, vScrollBar, indicator, filename)
    {}

    // Override mapColor so the base-class draw() uses our background colour
    // for normal (index 1) and selected (index 2) text.
    TColorAttr mapColor(uchar index) noexcept override {
        switch (index) {
            case 1: return TColorAttr(TColorRGB(0xC0C0C0), TColorRGB(0x000080)); // normal
            case 2: return TColorAttr(TColorRGB(0x000000), TColorRGB(0x00AAAA)); // selected
            default: return TFileEditor::mapColor(index);
        }
    }

    // draw() is virtual on TView.  We call the base-class draw() first so
    // that the editor content, cursor, and selection are all rendered
    // correctly.  Then we do a second pass over every visible column of
    // every visible line: we re-read the raw gap buffer, re-tokenise, and
    // overwrite the colour attribute of non-selected characters with the
    // appropriate syntax colour using writeChar().
    //
    // writeChar(x, y, ch, colorIndex, count) uses palette index → mapColor(),
    // but we want literal TColorAttr values.  Instead we build a one-cell
    // TDrawBuffer and call writeBuf(), which accepts TColorAttr directly.
    void draw() override {
        // 1. Let the base class do its full draw (text, cursor, selection).
        TFileEditor::draw();

        // 2. Determine visible line range.
        //    delta.y = first visible logical line index,  size.y = visible rows.
        int firstLine = delta.y;          // topmost visible line
        int visRows   = size.y;
        int visCols   = size.x;
        int firstCol  = delta.x;          // horizontal scroll offset

        // Selection range (byte offsets, normalised lo<=hi)
        uint selLo = (selStart <= selEnd) ? selStart : selEnd;
        uint selHi = (selStart <= selEnd) ? selEnd   : selStart;

        // Helper: read logical char index i from the gap buffer
        auto gapChar = [&](uint i) -> char {
            return buffer[ i < curPtr ? i : i + gapLen ];
        };

        // Walk to the first visible line
        uint lineStart = 0;             // byte offset of start of current line
        int  lineNo    = 0;
        // Scan from beginning to find byte offset of firstLine
        // (We walk the whole buffer once; acceptable since files are small)
        while (lineNo < firstLine && lineStart < bufLen) {
            if (gapChar(lineStart) == '\n') lineNo++;
            lineStart++;
        }

        // Now render each visible row
        for (int row = 0; row < visRows && lineStart <= bufLen; row++) {
            // Find end of this logical line
            uint lineEnd = lineStart;
            while (lineEnd < bufLen && gapChar(lineEnd) != '\n') lineEnd++;

            // Tokenise and write re-coloured characters for this line
            // We only touch columns [firstCol .. firstCol+visCols)
            uint i = lineStart;
            int  screenCol = 0;  // column index relative to visible area

            // Helper: colour one char at buffer offset i, screen col sc
            auto recolour = [&](uint offset, char ch, TColorAttr ca) {
                // Skip selected characters — leave them as drawn by base class
                if (offset >= selLo && offset < selHi) return;
                int sc = (int)(offset - lineStart) - firstCol;
                if (sc < 0 || sc >= visCols) return;
                TDrawBuffer b;
                b.moveChar(0, ch, ca, 1);
                writeBuf(sc, row, 1, 1, b);
            };

            while (i < lineEnd) {
                char c = gapChar(i);

                // { ... } block comment
                if (c == '{') {
                    uint start = i++;
                    while (i < lineEnd && gapChar(i) != '}') i++;
                    if (i < lineEnd) i++;
                    TColorAttr ca = TColorAttr(TColorRGB(0x55FFFF), TColorRGB(0x000080));
                    for (uint j = start; j < i; j++) recolour(j, gapChar(j), ca);
                    continue;
                }
                // (* ... *) comment
                if (c == '(' && i+1 < lineEnd && gapChar(i+1) == '*') {
                    uint start = i; i += 2;
                    while (i < lineEnd) {
                        if (gapChar(i) == '*' && i+1 < lineEnd && gapChar(i+1) == ')') { i += 2; break; }
                        i++;
                    }
                    TColorAttr ca = TColorAttr(TColorRGB(0x55FFFF), TColorRGB(0x000080));
                    for (uint j = start; j < i; j++) recolour(j, gapChar(j), ca);
                    continue;
                }
                // // line comment
                if (c == '/' && i+1 < lineEnd && gapChar(i+1) == '/') {
                    TColorAttr ca = TColorAttr(TColorRGB(0x55FFFF), TColorRGB(0x000080));
                    for (uint j = i; j < lineEnd; j++) recolour(j, gapChar(j), ca);
                    i = lineEnd;
                    continue;
                }
                // String/char literal '...'
                if (c == '\'') {
                    uint start = i++;
                    while (i < lineEnd) { if (gapChar(i++) == '\'') break; }
                    TColorAttr ca = TColorAttr(TColorRGB(0x55FF55), TColorRGB(0x000080));
                    for (uint j = start; j < i; j++) recolour(j, gapChar(j), ca);
                    continue;
                }
                // Hex or decimal number
                if (isdigit((unsigned char)c) ||
                    (c == '$' && i+1 < lineEnd && isxdigit((unsigned char)gapChar(i+1)))) {
                    uint start = i;
                    while (i < lineEnd && (isalnum((unsigned char)gapChar(i)) || gapChar(i) == '.')) i++;
                    TColorAttr ca = TColorAttr(TColorRGB(0x55FFFF), TColorRGB(0x000080));
                    for (uint j = start; j < i; j++) recolour(j, gapChar(j), ca);
                    continue;
                }
                // Identifier or keyword
                if (isalpha((unsigned char)c) || c == '_') {
                    uint start = i;
                    while (i < lineEnd && (isalnum((unsigned char)gapChar(i)) || gapChar(i) == '_')) i++;
                    char word[64]; int wlen = std::min((int)(i - start), 63);
                    for (int k = 0; k < wlen; k++) word[k] = gapChar(start + k);
                    word[wlen] = '\0';
                    if (isPascalKeyword(word, wlen)) {
                        TColorAttr ca = TColorAttr(TColorRGB(0xFFFF55), TColorRGB(0x000080));
                        for (uint j = start; j < i; j++) recolour(j, gapChar(j), ca);
                    }
                    // non-keywords: leave base-class colour (already correct)
                    continue;
                }
                i++;
            }

            // Advance to next line
            lineStart = lineEnd + 1;  // +1 to skip the '\n'
        }
    }
};


// ── Editor Window ─────────────────────────────────────────────────────────
class TEditorWindow : public TWindow {
public:
    TPascalEditor* editor;
    TIndicator*    indicator;
    char         filename[MAXPATH];

    TEditorWindow(TRect bounds, const char* aFile)
        : TWindowInit(&TEditorWindow::initFrame),
          TWindow(bounds,
                  (aFile && *aFile) ? aFile : " Untitled ",
                  wnNoNumber)
    {
        options |= ofTileable;
        TScrollBar* vbar = new TScrollBar(TRect(size.x-1, 1,        size.x,   size.y-1));
        TScrollBar* hbar = new TScrollBar(TRect(1,        size.y-1, size.x-1, size.y));
        insert(vbar);
        insert(hbar);
        indicator = new TIndicator(TRect(0, size.y-1, 12, size.y));
        insert(indicator);
        TRect r(1, 1, size.x-1, size.y-1);
        if (aFile && *aFile) {
            strncpy(filename, aFile, MAXPATH-1);
            filename[MAXPATH-1] = '\0';
        } else {
            filename[0] = '\0';
        }
        editor = new TPascalEditor(r, hbar, vbar, indicator,
                                   filename[0] ? TStringView(filename) : TStringView());
        insert(editor);
        editor->select();
    }

    const char* getTitle(short /*maxSize*/) override {
        return filename[0] ? filename : " Untitled ";
    }

    // Write gap buffer to disk directly (works for both new and existing files)
    bool writeBufferToFile(const char* path) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        // Gap buffer layout: [0..curPtr) | gap | (curPtr+gapLen..bufLen+gapLen)
        if (editor->curPtr > 0)
            f.write(editor->buffer, editor->curPtr);
        uint postStart = editor->curPtr + editor->gapLen;
        uint total     = editor->bufLen + editor->gapLen;
        if (postStart < total)
            f.write(editor->buffer + postStart, total - postStart);
        return f.good();
    }

    bool save() {
        if (filename[0] == '\0') return saveAs();
        if (writeBufferToFile(filename)) {
            editor->modified = False;
            return true;
        }
        messageBox(" Error saving file! ", mfError | mfOKButton);
        return false;
    }

    bool saveAs() {
        TFileDialog* dlg = new TFileDialog("*.pas", " Save As ",
                                           "~N~ame", fdOKButton, 100);
        bool ok = false;
        if (TProgram::deskTop->execView(dlg) != cmCancel) {
            char buf[MAXPATH] = {};
            dlg->getFileName(buf);
            strncpy(filename, buf, MAXPATH-1);
            if (writeBufferToFile(filename)) {
                editor->modified = False;
                ok = true;
                if (frame) frame->drawView();
            } else {
                messageBox(" Error saving file! ", mfError | mfOKButton);
            }
        }
        TObject::destroy(dlg);
        return ok;
    }

    // Override valid() so that cmQuit / cmClose prompt to save if modified,
    // then allow the close. Without this, TEditor blocks quit silently.
    Boolean valid(ushort command) override {
        if (command == cmQuit || command == cmClose) {
            if (editor->modified) {
                // Ask user
                int r = messageBox(
                    " File has unsaved changes. Save before closing?",
                    mfYesNoCancel
                );
                if (r == cmYes)   return Boolean(save());
                if (r == cmNo)    return True;
                return False; // Cancel
            }
        }
        return TWindow::valid(command);
    }

    std::string getFilePath() {
        if (filename[0] == '\0') {
            if (!saveAs()) return "";
        } else {
            save();
        }
        return std::string(filename);
    }

    void jumpToLine(int targetLine) {
        if (targetLine <= 0) { editor->setCurPtr(0, 0); editor->drawView(); return; }
        uint line = 0;
        uint len  = editor->bufLen;
        for (uint i = 0; i < len; i++) {
            uint phys = (i < editor->curPtr) ? i : i + editor->gapLen;
            if (editor->buffer[phys] == '\n') {
                line++;
                if ((int)line == targetLine) {
                    editor->setCurPtr(i + 1, 0);
                    editor->drawView();
                    return;
                }
            }
        }
        editor->setCurPtr(len, 0);
        editor->drawView();
    }

    void handleEvent(TEvent& event) override {
        if (event.what == evCommand) {
            switch (event.message.command) {
                case cmSaveFile:   save();   clearEvent(event); return;
                case cmSaveFileAs: saveAs(); clearEvent(event); return;
            }
        }
        TWindow::handleEvent(event);
    }
};

// ── Application ───────────────────────────────────────────────────────────
class TPascalIDE : public TApplication {
public:
    TPascalIDE();
    void handleEvent(TEvent& event) override;
    static TMenuBar*    initMenuBar(TRect r);
    static TStatusLine* initStatusLine(TRect r);
private:
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void runProgram();
    void showAbout();
    void gotoLine();
    TEditorWindow* activeEditor();
};

TPascalIDE::TPascalIDE()
    : TProgInit(&TPascalIDE::initStatusLine,
                &TPascalIDE::initMenuBar,
                &TPascalIDE::initDeskTop)
{
    newFile();
}

TMenuBar* TPascalIDE::initMenuBar(TRect r) {
    r.b.y = r.a.y + 1;
    return new TMenuBar(r,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("~N~ew",           cmNewFile,    kbNoKey,    hcNoContext, "F2")        +
            *new TMenuItem("~O~pen...",       cmOpenFile,   kbF3,       hcNoContext, "F3")        +
            *new TMenuItem("~S~ave",          cmSaveFile,   kbF2,       hcNoContext, "F2")        +
            *new TMenuItem("Save ~A~s...",    cmSaveFileAs, kbNoKey)                              +
            newLine()                                                                              +
            *new TMenuItem("E~x~it",          cmQuit,       kbAltX,     hcNoContext, "Alt-X")     +
        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("~U~ndo",          cmUndo,       kbAltBack,  hcNoContext, "Alt-BkSp")  +
            newLine()                                                                              +
            *new TMenuItem("~C~ut",           cmCut,        kbShiftDel, hcNoContext, "Shift-Del") +
            *new TMenuItem("C~o~py",          cmCopy,       kbCtrlIns,  hcNoContext, "Ctrl-Ins")  +
            *new TMenuItem("~P~aste",         cmPaste,      kbShiftIns, hcNoContext, "Shift-Ins") +
            newLine()                                                                              +
            *new TMenuItem("~G~o to Line...", cmGotoLine,   kbNoKey)                              +
        *new TSubMenu("~R~un", kbAltR) +
            *new TMenuItem("~R~un",           cmRunProgram, kbF9,       hcNoContext, "F9")        +
        *new TSubMenu("~H~elp", kbAltH) +
            *new TMenuItem("~A~bout...",      cmAbout,      kbNoKey)
    );
}

TStatusLine* TPascalIDE::initStatusLine(TRect r) {
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~F2~ Save",    kbF2,   cmSaveFile)   +
            *new TStatusItem("~F3~ Open",    kbF3,   cmOpenFile)   +
            *new TStatusItem("~F9~ Run",     kbF9,   cmRunProgram) +
            *new TStatusItem("~F10~ Menu",   kbF10,  cmMenu)       +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit)
    );
}

TEditorWindow* TPascalIDE::activeEditor() {
    return dynamic_cast<TEditorWindow*>(deskTop->current);
}

void TPascalIDE::handleEvent(TEvent& event) {
    // Handle our custom commands first; let everything else (cmQuit etc)
    // fall through to TApplication which knows how to handle them.
    if (event.what == evCommand) {
        switch (event.message.command) {
            case cmNewFile:    newFile();    clearEvent(event); return;
            case cmOpenFile:   openFile();   clearEvent(event); return;
            case cmSaveFile:   saveFile();   clearEvent(event); return;
            case cmSaveFileAs: saveFileAs(); clearEvent(event); return;
            case cmRunProgram: runProgram(); clearEvent(event); return;
            case cmAbout:      showAbout();  clearEvent(event); return;
            case cmGotoLine:   gotoLine();   clearEvent(event); return;
        }
    }
    TApplication::handleEvent(event);
}

void TPascalIDE::newFile() {
    TRect r = deskTop->getExtent();
    r.grow(-2, -1);
    deskTop->insert(new TEditorWindow(r, nullptr));
}

void TPascalIDE::openFile() {
    TFileDialog* dlg = new TFileDialog("*.pas", " Open Pascal File ",
                                       "~N~ame", fdOpenButton, 100);
    if (deskTop->execView(dlg) != cmCancel) {
        char buf[MAXPATH] = {};
        dlg->getFileName(buf);
        TRect r = deskTop->getExtent();
        r.grow(-2, -1);
        deskTop->insert(new TEditorWindow(r, buf));
    }
    TObject::destroy(dlg);
}

void TPascalIDE::saveFile()   { if (auto* w = activeEditor()) w->save(); }
void TPascalIDE::saveFileAs() { if (auto* w = activeEditor()) w->saveAs(); }

void TPascalIDE::runProgram() {
    auto* w = activeEditor();
    if (!w) { messageBox(" No editor open. ", mfError | mfOKButton); return; }
    std::string path = w->getFilePath();
    if (path.empty()) return;
    // Run interactively so the program can use stdin/stdout
    runPascalInteractive(path.c_str());
}

void TPascalIDE::showAbout() {
    auto* dlg = new TDialog(TRect(0,0,50,11), " About Pascal IDE ");
    dlg->options |= ofCentered;
    dlg->insert(new TStaticText(TRect(2,2,48,3), " Pascal IDE  v1.0"));
    dlg->insert(new TStaticText(TRect(2,3,48,4), " Built on tvision (magiblot)"));
    dlg->insert(new TStaticText(TRect(2,5,48,6), " F2  Save    F3  Open"));
    dlg->insert(new TStaticText(TRect(2,6,48,7), " F9  Run     F10 Menu"));
    dlg->insert(new TStaticText(TRect(2,7,48,8), " Alt-X  Exit"));
    dlg->insert(new TButton(TRect(20,9,30,10), "  ~O~K  ", cmOK, bfDefault));
    deskTop->execView(dlg);
    TObject::destroy(dlg);
}

void TPascalIDE::gotoLine() {
    auto* w = activeEditor();
    if (!w) return;
    auto* dlg = new TDialog(TRect(0,0,36,8), " Go to Line ");
    dlg->options |= ofCentered;
    auto* inp = new TInputLine(TRect(2,3,34,4), 10);
    dlg->insert(new TLabel(TRect(2,2,20,3), "~L~ine number:", inp));
    dlg->insert(inp);
    dlg->insert(new TButton(TRect(4,5,14,6),  " ~O~K ",     cmOK,     bfDefault));
    dlg->insert(new TButton(TRect(16,5,28,6), " ~C~ancel ", cmCancel, bfNormal));
    dlg->selectNext(False);
    if (deskTop->execView(dlg) != cmCancel) {
        char buf[16] = {};
        inp->getData(buf);
        int line = atoi(buf);
        if (line > 0) w->jumpToLine(line - 1);
    }
    TObject::destroy(dlg);
}

int main(int argc, char* argv[]) {
    TPascalIDE app;
    if (argc >= 2) {
        TRect r = app.deskTop->getExtent();
        r.grow(-2, -1);
        app.deskTop->insert(new TEditorWindow(r, argv[1]));
    }
    app.run();
    return 0;
}
