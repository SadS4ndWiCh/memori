#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define MEMORI_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum EditorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DELETE_KEY
};

/* Editor Row */
typedef struct erow {
    int size;
    char *chars;
} erow;

/* Global editor configurations */
struct EditorConfig {
    int cx, cy;

    int screenRows;
    int screenCols;

    int numRows;
    erow row;

    /* Original terminal state. */
    struct termios terminal;
};

struct EditorConfig editorConfig;

void Terminal_die(const char *message) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(message);
    exit(1);
}

void Terminal_disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editorConfig.terminal) == -1) {
        Terminal_die("tcsetattr");
    }
}

void Terminal_enableRawMode(void) {
    /*
        Get the current state from terminal with `tcgetattr` and set to
        `termios` struct to be able to manipule it.

        To restore to initial state, we call the `Terminal_disableRawMode` when 
        `exit` is called.
    */
    if (tcgetattr(STDIN_FILENO, &editorConfig.terminal) == -1) {
        Terminal_die("tcgetattr");
    }
    atexit(Terminal_disableRawMode);

    /* 
        `c_lflag` means `local flags` that describe other states:
            - `ECHO` flag say to print each key you type to the terminal.
            - `ICANON` flag say to input be available line by line.
            - `ISIG` flag generate the corresponding signal from keys.
            - `IEXTEN` flag say to terminal wait you type another key to send it 
            when you type `CTRL-V` first.
            - `BRKINT` flag say to emit a `SIGINT` signal with a break condition.
            - `INPCK` flag enables parity checking, which doesn’t seem to apply 
            to modern terminal emulators.
            - `ISTRIP` flag causes the 8th bit of each input byte to be stripped, 
            meaning it will set it to 0. This is probably already turned off.
        
        `c_iflag` means `input flags` that describe other states:
            - `IXON` flag enable the `CTRL-S` and `CTRL-Q` to stop and resume 
            from receiving input respectively.
            - `ICRNL` flag translate carriage return to newline on input. [termios(3)]
        
        `c_oflag` means `output flags` that describe other states:
            - `OPOST` flag translate each newline ("\n") we print into a carriage 
            return followed by a newline ("\r\n").
        
        `c_cflag` means `contant flags` that describe some constants:
            - `CSIZE` is the character size mask. Values are `CS5`, `CS6`, `CS7` or `CS8`.
        
        To turn off some flag, you need to use bitwise operation.
    */
    struct termios raw = editorConfig.terminal;
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    /*
        `c_cc` means `control characters`, an array of bytes that controls various 
        terminal settings:
            - `VMIN` sets the minimum number of bytes of input needed before `read()`
            can return.
            - `VTIME` sets the maximum amount of time to wait before `read()` returns.
    */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    /*
        Set the modified state to the terminal. Using the `TCSAFLUSH` action, 
        the modification is applied after all pending output to be written.
    */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        Terminal_die("tcsetattr");
    }
}

int Terminal_readKey(void) {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            Terminal_die("read");
        }
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return c;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return c;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return c;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                    case '1':
                    case '7':
                        return HOME_KEY;
                    
                    case '4':
                    case '8':
                        return END_KEY;

                    case '2':
                    case '5':
                        return PAGE_UP;

                    case '6':
                        return PAGE_DOWN;

                    case '3':
                        return DELETE_KEY;
                    }
                }
            }

            switch (seq[1]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
            }
        }

        return c;
    }

    return c;
}

int Terminal_getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

/*
    Get the terminal window size.

    To get the terminal window size, a simple way is to use `ioctl()` with the 
    `TIOCGWINSZ` request. It returns a struct with `ws_row` and `ws_col`.

    Because this method doesn't work in some systems, as fallback, is just to use
    the cursor position. To do that, set the cursor position to the end of 
    row and col, with `C` escape sequence (Cursor Forward) and `B` escape sequence 
    (Cursor Down), and then is just get the current position.
*/
int Terminal_getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }

        return Terminal_getCursorPosition(rows, cols);
    }

    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

struct AppendBuffer {
    char *buf;
    int len;
};

#define APPEND_BUFFER_INIT {NULL, 0}

void AppendBuffer_append(struct AppendBuffer *ab, const char *s, int len) {
    char *new = realloc(ab->buf, ab->len + len);
    if (!new) return;

    memcpy(&new[ab->len], s, len);
    ab->buf = new;
    ab->len += len;
}

void AppendBuffer_free(struct AppendBuffer *ab) {
    free(ab->buf);
}

/*
    Open a file in the editor.
*/
void Editor_open(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) Terminal_die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen = getline(&line, &linecap, fp);
    if (linelen != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\r' || line[linelen - 1] == '\n')) {
            linelen--;
        }

        editorConfig.row.size = linelen;
        editorConfig.row.chars = (char *) malloc(linelen + 1);
        memcpy(editorConfig.row.chars, line, linelen);

        editorConfig.row.chars[linelen] = '\0';
        editorConfig.numRows = 1;
    }

    free(line);
    fclose(fp);
}

void Editor_processMoveCursor(int key) {
    switch (key) {
    case 'k':
    case ARROW_UP:
        if (editorConfig.cy > 0) editorConfig.cy--;
        break;
    case 'j':
    case ARROW_DOWN:
        if (editorConfig.cy < editorConfig.screenRows - 1) editorConfig.cy++;
        break;
    case 'l':
    case ARROW_RIGHT:
        if (editorConfig.cx < editorConfig.screenCols - 1) editorConfig.cx++;
        break;
    case 'h':
    case ARROW_LEFT:
        if (editorConfig.cx > 0) editorConfig.cx--;
        break;
    }
}

void Editor_processKey(void) {
    int key = Terminal_readKey();

    switch (key) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        exit(0);
        break;

    case PAGE_UP:
    case PAGE_DOWN:
        {
            int times = editorConfig.screenRows;
            while (times--) {
                Editor_processMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
        }
        break;
    
    case HOME_KEY:
        editorConfig.cx = 0;
        break;

    case END_KEY:
        editorConfig.cx = editorConfig.screenCols - 1;
        break;

    case 'k':
    case 'j':
    case 'l':
    case 'h':
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_RIGHT:
    case ARROW_LEFT:
        Editor_processMoveCursor(key);
        break;
    }
}

void Editor_drawRows(struct AppendBuffer *ab) {
    for (int y = 0; y < editorConfig.screenRows; y++) {
        if (y < editorConfig.numRows) {
            int len = editorConfig.row.size;
            if (len > editorConfig.screenCols) {
                len = editorConfig.screenCols;
            }

            AppendBuffer_append(ab, editorConfig.row.chars, len);
        } else if (y == editorConfig.screenRows / 3) {
            char welcome[80];
            int welcomeLen = snprintf(welcome, sizeof(welcome), "Memori editor -- version %s", MEMORI_VERSION);

            if (welcomeLen > editorConfig.screenCols) {
                welcomeLen = editorConfig.screenCols;
            }

            int padding = (editorConfig.screenCols - welcomeLen) / 2;
            if (padding) {
                AppendBuffer_append(ab, "~", 1);
                padding--;
            }

            while (padding--) AppendBuffer_append(ab, " ", 1);

            AppendBuffer_append(ab, welcome, welcomeLen);
        } else {
            AppendBuffer_append(ab, "~", 1);
        }

        // The `K` (Erase In Line) escape sequence. With default argument (0), 
        // it erase the whole line after cursor
        AppendBuffer_append(ab, "\x1b[K", 3);
        if (y < editorConfig.screenRows - 1) {
            AppendBuffer_append(ab, "\r\n", 2);
        }
    }
}

/*
    Refresh the screen on every render.

    The refresh steps are:

    1. it hides the cursor with the `l` (Set Mode) espace sequence and 
    set cursor position to the top;
    2. Draw all rows;
    3. Go back to the top and shows the cursor with `h` (Reset Mode) escape sequence.
*/
void Editor_refreshScreen(void) {
    struct AppendBuffer ab = APPEND_BUFFER_INIT;

    AppendBuffer_append(&ab, "\x1b[?25l", 6);
    AppendBuffer_append(&ab, "\x1b[H", 3);

    Editor_drawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", editorConfig.cy + 1, editorConfig.cx + 1);
    AppendBuffer_append(&ab, buf, strlen(buf));

    AppendBuffer_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    AppendBuffer_free(&ab);
}

void Editor_init(void) {
    editorConfig.cx = 0;
    editorConfig.cy = 0;
    editorConfig.numRows = 0;

    if (Terminal_getWindowSize(&editorConfig.screenRows, &editorConfig.screenCols) == -1) {
        Terminal_die("getWindowSize");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: %s <file>\n", argv[0]);
        return 0;
    }

    Terminal_enableRawMode();
    Editor_init();
    Editor_open(argv[1]);

    while(1) {
        Editor_refreshScreen();
        Editor_processKey();
    }

    return 0;
}