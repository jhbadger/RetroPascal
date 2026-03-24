/*
 * Pascal IDE — Turbo Pascal 7-style IDE using the tvision library
 *
 * Build (after building tvision):
 *   mkdir build && cd build
 *   cmake .. && make
 *
 * Or manually:
 *   g++ -std=c++17 -O2 -o pascal_ide pascal_ide.cpp \
 *       -I/path/to/tvision/include \
 *       -L/path/to/tvision/build -ltvision -lpthread
 *
 * Place the 'pascal' interpreter binary in the same directory or on PATH.
 *
 * Features:
 *   - Full tvision TApplication with TMenuBar + TStatusLine
 *   - TFileEditor window (open, save, save-as)
 *   - F9 Run: saves, executes via the pascal interpreter, shows output
 *   - Output window (TScroller inside TWindow)
 *   - File/Edit/Run/Help menus with standard keyboard shortcuts
 *   - About dialog
 */

// ── tvision uses-macros ────────────────────────────────────────────────────
#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_TDesktop
#define Uses_TDialog
#define Uses_TEditor
#define Uses_TEvent
#define Uses_TFileDialog
#define Uses_TFileEditor
#define Uses_THistory
#define Uses_TInputLine
#define Uses_TKeys
#define Uses_TLabel
#define Uses_TMenuBar
#define Uses_TMenuItem
#define Uses_TMsgBoxParam
#define Uses_TProgram
#define Uses_TRect
#define Uses_TScreen
#define Uses_TScrollBar
#define Uses_TStaticText
#define Uses_TStatusDef
#define Uses_TStatusItem
#define Uses_TStatusLine
#define Uses_TStreamable
#define Uses_TSubMenu
#define Uses_TWindow
#define Uses_TButton
#define Uses_MsgBox
#define Uses_TTerminal
#define Uses_otstream
#define Uses_TDrawBuffer
#define Uses_TScroller
#include <tvision/tv.h>

// ── Standard library ──────────────────────────────────────────────────────
#include <cstring>
#include <string>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// ── Custom command IDs ────────────────────────────────────────────────────
const ushort
    cmNewFile    = 101,
    cmOpenFile   = 102,
    cmSaveFile   = 103,
    cmSaveAs     = 104,
    cmRunProgram = 105,
    cmShowOutput = 106,
    cmAbout      = 107,
    cmGotoLine   = 108;

// ── Run the pascal interpreter and capture output ─────────────────────────
struct RunResult {
    std::string output;
    bool        success;
};

static RunResult runPascal(const char* filepath) {
    RunResult res;

    // Locate the interpreter: same dir as this binary, or on PATH
    const char* candidates[] = { "./pascal", "pascal", nullptr };
    std::string interp;
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) { interp = candidates[i]; break; }
    }
    if (interp.empty()) {
        res.output  = "Error: 'pascal' interpreter not found.\n"
                      "Place it in the same directory as pascal_ide.";
        res.success = false;
        return res;
    }

    // Pipe for combined stdout+stderr
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        res.output = "Error: pipe() failed."; res.success = false; return res;
    }

    pid_t pid = fork();
    if (pid < 0) {
        res.output = "Error: fork() failed."; res.success = false; return res;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        // stdin from /dev/null so the program doesn't block waiting for input
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) { dup2(null_fd, STDIN_FILENO); close(null_fd); }
        close(pipefd[1]);
        execlp(interp.c_str(), interp.c_str(), filepath, nullptr);
        _exit(127);
    }
    close(pipefd[1]);

    // Read all output
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        res.output.append(buf, n);
    close(pipefd[0]);

    int status = 0;
    // Wait up to 10 seconds
    for (int ms = 0; ms < 10000; ms += 10) {
        if (waitpid(pid, &status, WNOHANG) == pid) goto done;
        usleep(10000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    res.output += "\n[Execution timed out after 10s]";
done:
    res.success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return res;
}

// ── Output Window ─────────────────────────────────────────────────────────
//  A simple modal dialog that shows multiline text in a scrollable view.

class TOutputDialog : public TDialog {
public:
    TOutputDialog(const std::string& text, bool success)
        : TDialog(TRect(2, 1, 78, 22),
                  success ? " Program Output " : " Output / Errors ")
    {
        options |= ofCentered;

        // Scrollbars
        TScrollBar* vbar = new TScrollBar(TRect(size.x-2, 1, size.x-1, size.y-3));
        insert(vbar);
        TScrollBar* hbar = new TScrollBar(TRect(1, size.y-3, size.x-2, size.y-2));
        insert(hbar);

        // Terminal widget to display the text
        TRect r(1, 1, size.x-2, size.y-3);
        auto* term = new TTerminal(r, hbar, vbar, 32768);
        insert(term);

        // Push text into the terminal
        // TTerminal accepts a stream via otstream
        otstream os(term);
        os << text;
        if (!text.empty() && text.back() != '\n') os << '\n';
        os.flush();

        // OK button
        TButton* ok = new TButton(TRect((size.x-10)/2, size.y-2, (size.x+10)/2, size.y-1),
                                  "  ~O~K  ", cmOK, bfDefault);
        insert(ok);

        selectNext(False);
    }
};

// ── Editor Window ─────────────────────────────────────────────────────────
//  Wraps TFileEditor in a properly-sized TWindow.

class TEditorWindow : public TWindow {
public:
    TFileEditor* editor;
    char         filename[MAXPATH];

    TEditorWindow(TRect bounds, const char* aFile)
        : TWindow(bounds, aFile && *aFile ? aFile : " Untitled ", wnNoNumber)
    {
        options |= ofTileable;
        TScrollBar* vbar = new TScrollBar(TRect(size.x-1, 1, size.x, size.y-1));
        insert(vbar);
        TScrollBar* hbar = new TScrollBar(TRect(1, size.y-1, size.x-1, size.y));
        insert(hbar);

        TRect r(1, 1, size.x-1, size.y-1);
        if (aFile && *aFile) {
            editor = new TFileEditor(r, hbar, vbar, aFile);
            strncpy(filename, aFile, MAXPATH-1);
        } else {
            editor = new TFileEditor(r, hbar, vbar, nullptr);
            filename[0] = '\0';
        }
        insert(editor);
        editor->select();
    }

    // Save with current filename; returns false if user cancelled
    bool save() {
        if (filename[0] == '\0') return saveAs();
        return editor->saveFile() != 0;
    }

    bool saveAs() {
        TFileDialog* dlg = new TFileDialog("*.pas", " Save As ", "~N~ame",
                                           fdOKButton, 100);
        if (filename[0]) dlg->setFileName(filename);
        bool ok = false;
        if (TProgram::deskTop->execView(dlg) != cmCancel) {
            char buf[MAXPATH];
            dlg->getFileName(buf);
            strncpy(filename, buf, MAXPATH-1);
            // Update window title
            setTitle(filename);
            frame->drawView();
            ok = editor->saveFile() != 0;
        }
        TObject::destroy(dlg);
        return ok;
    }

    // Return full path of the saved file (saves first if needed)
    std::string getFilePath() {
        if (filename[0] == '\0') {
            if (!saveAs()) return "";
        } else {
            save();
        }
        return std::string(filename);
    }

    void handleEvent(TEvent& event) override {
        TWindow::handleEvent(event);
        if (event.what == evCommand) {
            switch (event.message.command) {
                case cmSaveFile: save();   clearEvent(event); break;
                case cmSaveAs:   saveAs(); clearEvent(event); break;
            }
        }
    }
};

// ── Main Application ──────────────────────────────────────────────────────

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
    void saveAsFile();
    void runProgram();
    void showAbout();
    void gotoLine();

    TEditorWindow* activeEditor();
};

// ── Constructor ───────────────────────────────────────────────────────────
TPascalIDE::TPascalIDE()
    : TProgInit(&TPascalIDE::initStatusLine,
                &TPascalIDE::initMenuBar,
                &TPascalIDE::initDeskTop)
{
    // Open a blank editor on startup
    newFile();
}

// ── Menu Bar ──────────────────────────────────────────────────────────────
TMenuBar* TPascalIDE::initMenuBar(TRect r) {
    r.b.y = r.a.y + 1;
    return new TMenuBar(r,
        *new TSubMenu("~F~ile", kbAltF) +
            *new TMenuItem("~N~ew",        cmNewFile,    kbF2,       hcNoContext, "F2")    +
            *new TMenuItem("~O~pen...",    cmOpenFile,   kbF3,       hcNoContext, "F3")    +
            *new TMenuItem("~S~ave",       cmSaveFile,   kbF2,       hcNoContext, "F2")    +
            *new TMenuItem("Save ~A~s...", cmSaveAs,     kbNoKey)    +
            newLine()                                                                       +
            *new TMenuItem("E~x~it",       cmQuit,       kbAltX,     hcNoContext, "Alt-X") +

        *new TSubMenu("~E~dit", kbAltE) +
            *new TMenuItem("~U~ndo",       cmUndo,       kbAltBack,  hcNoContext, "Alt-BkSp") +
            newLine()                                                                          +
            *new TMenuItem("~C~ut",        cmCut,        kbShiftDel, hcNoContext, "Shift-Del") +
            *new TMenuItem("C~o~py",       cmCopy,       kbCtrlIns,  hcNoContext, "Ctrl-Ins")  +
            *new TMenuItem("~P~aste",      cmPaste,      kbShiftIns, hcNoContext, "Shift-Ins") +
            newLine()                                                                           +
            *new TMenuItem("~G~oto Line...", cmGotoLine, kbNoKey)                             +

        *new TSubMenu("~R~un", kbAltR) +
            *new TMenuItem("~R~un",        cmRunProgram, kbF9,       hcNoContext, "F9")    +

        *new TSubMenu("~H~elp", kbAltH) +
            *new TMenuItem("~A~bout...",   cmAbout,      kbNoKey)
    );
}

// ── Status Line ───────────────────────────────────────────────────────────
TStatusLine* TPascalIDE::initStatusLine(TRect r) {
    r.a.y = r.b.y - 1;
    return new TStatusLine(r,
        *new TStatusDef(0, 0xFFFF) +
            *new TStatusItem("~F2~ Save",  kbF2,  cmSaveFile)   +
            *new TStatusItem("~F3~ Open",  kbF3,  cmOpenFile)   +
            *new TStatusItem("~F9~ Run",   kbF9,  cmRunProgram) +
            *new TStatusItem("~F10~ Menu", kbF10, cmMenu)       +
            *new TStatusItem("~Alt-X~ Exit", kbAltX, cmQuit)
    );
}

// ── Helper: get the active TEditorWindow ─────────────────────────────────
TEditorWindow* TPascalIDE::activeEditor() {
    return dynamic_cast<TEditorWindow*>(deskTop->current);
}

// ── Event Handler ─────────────────────────────────────────────────────────
void TPascalIDE::handleEvent(TEvent& event) {
    TApplication::handleEvent(event);

    if (event.what == evCommand) {
        switch (event.message.command) {
            case cmNewFile:    newFile();    clearEvent(event); break;
            case cmOpenFile:   openFile();   clearEvent(event); break;
            case cmSaveFile:   saveFile();   clearEvent(event); break;
            case cmSaveAs:     saveAsFile(); clearEvent(event); break;
            case cmRunProgram: runProgram(); clearEvent(event); break;
            case cmAbout:      showAbout();  clearEvent(event); break;
            case cmGotoLine:   gotoLine();   clearEvent(event); break;
        }
    }
}

// ── Actions ───────────────────────────────────────────────────────────────
void TPascalIDE::newFile() {
    TRect r = deskTop->getExtent();
    r.grow(-2, -1);
    TEditorWindow* w = new TEditorWindow(r, nullptr);
    deskTop->insert(w);
}

void TPascalIDE::openFile() {
    TFileDialog* dlg = new TFileDialog("*.pas", " Open Pascal File ",
                                       "~N~ame", fdOpenButton, 100);
    if (deskTop->execView(dlg) != cmCancel) {
        char filename[MAXPATH];
        dlg->getFileName(filename);
        TRect r = deskTop->getExtent();
        r.grow(-2, -1);
        TEditorWindow* w = new TEditorWindow(r, filename);
        deskTop->insert(w);
    }
    TObject::destroy(dlg);
}

void TPascalIDE::saveFile() {
    TEditorWindow* w = activeEditor();
    if (w) w->save();
}

void TPascalIDE::saveAsFile() {
    TEditorWindow* w = activeEditor();
    if (w) w->saveAs();
}

void TPascalIDE::runProgram() {
    TEditorWindow* w = activeEditor();
    if (!w) {
        messageBox(" No editor open. ", mfError | mfOKButton);
        return;
    }

    std::string path = w->getFilePath();
    if (path.empty()) return; // user cancelled save-as

    // Run
    RunResult res = runPascal(path.c_str());

    // Show output in a dialog
    TOutputDialog* dlg = new TOutputDialog(res.output, res.success);
    deskTop->execView(dlg);
    TObject::destroy(dlg);
}

void TPascalIDE::showAbout() {
    TDialog* dlg = new TDialog(TRect(0, 0, 50, 12), " About Pascal IDE ");
    dlg->options |= ofCentered;

    dlg->insert(new TStaticText(TRect(2, 2, 48, 3),  " Pascal IDE  v1.0"));
    dlg->insert(new TStaticText(TRect(2, 3, 48, 4),  " Powered by tvision (magiblot)"));
    dlg->insert(new TStaticText(TRect(2, 4, 48, 5),  " "));
    dlg->insert(new TStaticText(TRect(2, 5, 48, 6),  " F9       Run program"));
    dlg->insert(new TStaticText(TRect(2, 6, 48, 7),  " F2       Save"));
    dlg->insert(new TStaticText(TRect(2, 7, 48, 8),  " F3       Open file"));
    dlg->insert(new TStaticText(TRect(2, 8, 48, 9),  " Alt-X    Exit"));

    dlg->insert(new TButton(TRect(20, 10, 30, 11), "  ~O~K  ", cmOK, bfDefault));

    deskTop->execView(dlg);
    TObject::destroy(dlg);
}

void TPascalIDE::gotoLine() {
    TEditorWindow* w = activeEditor();
    if (!w) return;

    // Simple input dialog for line number
    TDialog*   dlg   = new TDialog(TRect(0,0,36,8), " Go to Line ");
    dlg->options |= ofCentered;
    dlg->insert(new TLabel(TRect(2,2,18,3), "Line ~n~umber:", nullptr));
    TInputLine* inp = new TInputLine(TRect(18,2,33,3), 10);
    dlg->insert(inp);
    dlg->insert(new TButton(TRect(4,5,14,6),  " ~O~K ",     cmOK,     bfDefault));
    dlg->insert(new TButton(TRect(16,5,26,6), " ~C~ancel ", cmCancel, bfNormal));
    dlg->selectNext(False);

    if (deskTop->execView(dlg) != cmCancel) {
        char buf[16];
        inp->getData(buf);
        int line = atoi(buf);
        if (line > 0) {
            // TEditor::gotoLine is 0-based
            w->editor->gotoLine(line - 1);
        }
    }
    TObject::destroy(dlg);
}

// ── main ──────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    TPascalIDE app;

    // If a file was given on the command line, open it
    if (argc >= 2) {
        TRect r = app.deskTop->getExtent();
        r.grow(-2, -1);
        TEditorWindow* w = new TEditorWindow(r, argv[1]);
        app.deskTop->insert(w);
    }

    app.run();
    return 0;
}
