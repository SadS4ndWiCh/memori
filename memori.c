#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/* 
    Global original terminal state.
*/
struct termios terminal;

void Terminal_die(const char *message) {
    perror(message);
    exit(1);
}

void Terminal_disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) == -1) {
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
    if (tcgetattr(STDIN_FILENO, &terminal) == -1) {
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
    struct termios raw = terminal;
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

int main(int argc, char **argv) {
    Terminal_enableRawMode();

    /* 
        Read 1 byte from standart input.

        By default, the terminal starts in canonical mode (cooked mode), which 
        means that is required to press `ENTER` to send the input.

        Enter `q` to exit.
    */
    while(1) {
        char c = '\0';

        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) {
            Terminal_die("read");
        }

        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d (%c)\r\n", c, c);
        }

        if (c == 'q') break;
    }

    return 0;
}