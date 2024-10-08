#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

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
        `c_lflags` means `local flags` that describe other states:
            - `ECHO` flag say to print each key you type to the terminal.
            - `ICANON` flag say to input be available line by line.
        
        To turn off some flag, you need to use bitwise operation. Just negate 
        the `ECHO` flag.

        Now the terminal don't echo the each input character and can be read 
        byte by byte instead of line by line.
    */
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);

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
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');

    return 0;
}