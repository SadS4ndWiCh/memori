#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1f)

/* Global editor configurations */
struct EditorConfig {
    int screenRows;
    int screenCols;

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

char Terminal_readKey(void) {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) == 1) {
        if (nread == - 1 && errno != EAGAIN) {
            Terminal_die("read");
        }
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

void Editor_processKey(void) {
    char c = Terminal_readKey();

    switch (c) {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        exit(0);
        break;
    }
}

void Editor_drawRows(struct AppendBuffer *ab) {
    for (int y = 0; y < editorConfig.screenRows; y++) {
        AppendBuffer_append(ab, "~", 1);

        if (y < editorConfig.screenRows - 1) {
            AppendBuffer_append(ab, "\r\n", 2);
        }
    }
}

/*
    Refresh the screen on every render.

    To refresh the screen we use `scape sequences`, which is a sequence of bytes 
    starting with `\x1b` (27 in decimal) followed by a `[` character and the command.

    - `Erase in Display` -> erase(option) -> `\x1b[<option>J`:
        If `option` is 0, the screen would be cleaned from the cursor up to the 
        end of screen. If `option` is 1, the screen would be cleaned up to where 
        the cursor is. If `option` is 2, the entire screen would be cleaned.
    
    - `Cursor Position` -> position(row, col) -> `\x1b[<row>;<col>H`:
        The cursor position is set to coord `col`x`row`. Note: `row` and `col` 
        starts from 1.
*/
void Editor_refreshScreen(void) {
    struct AppendBuffer ab = APPEND_BUFFER_INIT;

    AppendBuffer_append(&ab, "\x1b[2J", 4);
    AppendBuffer_append(&ab, "\x1b[H", 3);

    Editor_drawRows(&ab);

    AppendBuffer_append(&ab, "\x1b[H", 3);

    write(STDOUT_FILENO, ab.buf, ab.len);
    AppendBuffer_free(&ab);
}

void Editor_init(void) {
    if (Terminal_getWindowSize(&editorConfig.screenRows, &editorConfig.screenCols) == -1) {
        Terminal_die("getWindowSize");
    }
}

int main(int argc, char **argv) {
    Terminal_enableRawMode();
    Editor_init();

    while(1) {
        Editor_refreshScreen();
        Editor_processKey();
    }

    return 0;
}