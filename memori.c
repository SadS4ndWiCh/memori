#include <unistd.h>
#include <termios.h>

void Terminal_enableRawMode(void) {
    /*
        Get the current state from terminal with `tcgetattr` and set to
        `termios` struct to be able to manipule it.
    */
    struct termios terminal;
    tcgetattr(STDIN_FILENO, &terminal);

    /* 
        `c_lflags` means `local flags` that describe other states:
            - `ECHO` flag say to print each key you type to the terminal.
        
        To turn off some flag, you need to use bitwise operation. Just negate 
        the `ECHO` flag.
    */
    terminal.c_lflag &= ~(ECHO);

    /*
        Set the modified state to the terminal. Using the `TCSAFLUSH` action, 
        the modification is applied after all pending output to be written.
    */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal);
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