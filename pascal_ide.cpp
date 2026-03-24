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

    // Map palette indices to colours.
    // TEditor uses index 1 (normal) and 2 (selected) internally.
    // We add 3=keyword, 4=comment, 5=string, 6=number.
    TColorAttr mapColor(uchar index) noexcept override {
        switch (index) {
            case 1: return TColorAttr(TColorRGB(0xC0C0C0), TColorRGB(0x000080)); // normal
            case 2: return TColorAttr(TColorRGB(0x000000), TColorRGB(0x00AAAA)); // selected
            case 3: return TColorAttr(TColorRGB(0xFFFF55), TColorRGB(0x000080)); // keyword
            case 4: return TColorAttr(TColorRGB(0x55FFFF), TColorRGB(0x000080)); // comment
            case 5: return TColorAttr(TColorRGB(0x55FF55), TColorRGB(0x000080)); // string
            case 6: return TColorAttr(TColorRGB(0x55FFFF), TColorRGB(0x000080)); // number
            default: return TFileEditor::mapColor(index);
        }
    }

    // Actual signature from editors.h:
    // void formatLine(TDrawBuffer& buf, uint linePtr, int lineLen, int lineWidth, TAttrPair colors)
    // linePtr  = byte offset into the editor buffer where this line starts
    // lineLen  = number of characters in the line
    // lineWidth = number of columns to fill (may be > lineLen due to scroll)
    // colors   = TAttrPair: low byte = normal attr index, high byte = selected attr index
    void formatLine(TDrawBuffer& buf, uint linePtr, int lineLen,
                    int lineWidth, TAttrPair colors) {
        // Extract the normal and selected colour attributes from the pair.
        // TAttrPair packs two TColorAttr values; we access them via mapColor.
        // colors low = palette index for normal text (usually 1)
        // colors high = palette index for selected text (usually 2)
        // If any part of the line is selected, the base class handles colouring
        // for the selected region. We only need to handle the non-selected case.
        //
        // Strategy: build our own colouring, then let the base class overlay
        // the selection on top by calling it with modified colors.
        // Actually, the cleanest approach: call base class first (which fills
        // buf with normal/selected colours), then re-colour non-selected chars
        // with syntax colour. But that doesn't work well.
        //
        // Better: replicate what the base class does but with our token colours.
        // The base class just calls buf.moveChar for each character.
        // We do the same but choose the colour based on token type.

        // Get the two colour attributes
        TColorAttr clrNormal   = mapColor(1);
        TColorAttr clrSelected = mapColor(2);

        // We need to know the selection range to correctly colour selected chars.
        // TEditor exposes: selStart, selEnd, curPtr (all byte offsets).
        // The selection is [min(selStart,selEnd) .. max(selStart,selEnd)).
        uint selLo = (selStart <= selEnd) ? selStart : selEnd;
        uint selHi = (selStart <= selEnd) ? selEnd   : selStart;

        // Walk the source text, tokenise, fill buf one character at a time.
        int col = 0;

        // Helper: write one character to buf at column col with a colour attr.
        // If the character is within the selection, always use clrSelected.
        auto putChar = [&](char ch, uint bufOffset, TColorAttr ca) {
            bool selected = (bufOffset >= selLo && bufOffset < selHi);
            buf.moveChar(col, ch, selected ? clrSelected : ca, 1);
            col++;
        };

        // Read characters from the gap buffer.
        // Physical index for logical index i: i < curPtr ? i : i + gapLen
        auto physChar = [&](int i) -> char {
            uint phys = ((uint)i < curPtr) ? (uint)i : (uint)i + gapLen;
            return buffer[phys];
        };

        int i = (int)linePtr;
        int end = i + lineLen;

        while (i < end && col < lineWidth) {
            char c = physChar(i);

            // { ... } block comment
            if (c == '{') {
                int start = i++;
                while (i < end && physChar(i) != '}') i++;
                if (i < end) i++;
                for (int j = start; j < i && col < lineWidth; j++)
                    putChar(physChar(j), linePtr + (j - (int)linePtr), mapColor(4));
                continue;
            }
            // (* ... *) comment
            if (c == '(' && i+1 < end && physChar(i+1) == '*') {
                int start = i; i += 2;
                while (i < end) {
                    if (physChar(i) == '*' && i+1 < end && physChar(i+1) == ')') { i += 2; break; }
                    i++;
                }
                for (int j = start; j < i && col < lineWidth; j++)
                    putChar(physChar(j), linePtr + (j - (int)linePtr), mapColor(4));
                continue;
            }
            // // line comment
            if (c == '/' && i+1 < end && physChar(i+1) == '/') {
                for (int j = i; j < end && col < lineWidth; j++)
                    putChar(physChar(j), linePtr + (j - (int)linePtr), mapColor(4));
                i = end;
                continue;
            }
            // String/char literal '...'
            if (c == '\'') {
                int start = i++;
                while (i < end) {
                    if (physChar(i++) == '\'') break;
                }
                for (int j = start; j < i && col < lineWidth; j++)
                    putChar(physChar(j), linePtr + (j - (int)linePtr), mapColor(5));
                continue;
            }
            // Hex or decimal number
            if (isdigit((unsigned char)c) ||
                (c == '$' && i+1 < end && isxdigit((unsigned char)physChar(i+1)))) {
                int start = i;
                while (i < end && (isalnum((unsigned char)physChar(i)) || physChar(i) == '.')) i++;
                for (int j = start; j < i && col < lineWidth; j++)
                    putChar(physChar(j), linePtr + (j - (int)linePtr), mapColor(6));
                continue;
            }
            // Identifier or keyword
            if (isalpha((unsigned char)c) || c == '_') {
                int start = i;
                while (i < end && (isalnum((unsigned char)physChar(i)) || physChar(i) == '_')) i++;
                // Copy word into a small buffer for keyword check
                char word[64]; int wlen = std::min(i - start, 63);
                for (int j = 0; j < wlen; j++) word[j] = physChar(start + j);
                word[wlen] = '\0';
                uchar tok = isPascalKeyword(word, wlen) ? 3 : 1;
                for (int j = start; j < i && col < lineWidth; j++)
                    putChar(physChar(j), linePtr + (j - (int)linePtr), mapColor(tok));
                continue;
            }
            // Everything else
            putChar(c, linePtr + (i - (int)linePtr), mapColor(1));
            i++;
        }
        // Pad remaining columns with spaces in normal colour
        while (col < lineWidth)
            buf.moveChar(col++, ' ', clrNormal, 1);
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
