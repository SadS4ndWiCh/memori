#include <unistd.h>

int main(int argc, char **argv) {
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