#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

/* 
    Global original terminal state.
*/
struct termios terminal;

void Terminal_disableRawMode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal);
}

void Terminal_enableRawMode(void) {
    /*
        Get the current state from terminal with `tcgetattr` and set to
        `termios` struct to be able to manipule it.

        To restore to initial state, we call the `Terminal_disableRawMode` when 
        `exit` is called.
    */
    tcgetattr(STDIN_FILENO, &terminal);
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
        Set the modified state to the terminal. Using the `TCSAFLUSH` action, 
        the modification is applied after all pending output to be written.
    */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char **argv) {
    Terminal_enableRawMode();

    char c;

    /* 
        Read 1 byte from standart input.

        By default, the terminal starts in canonical mode (cooked mode), which 
        means that is required to press `ENTER` to send the input.

        Enter `q` to exit.
    */
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d (%c)\r\n", c, c);
        }
    }

    return 0;
}