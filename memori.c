#include <unistd.h>

int main(int argc, char **argv) {
    char c;

    /* 
        Read 1 byte from standart input.

        By default, the terminal starts in canonical mode (cooked mode), which 
        means that is required to press `ENTER` to send the input.

        Until now, you need to press `CTRL-D` to send EOF or `CTRL-C` to 
        terminate.
    */
    while (read(STDIN_FILENO, &c, 1) == 1);

    return 0;
}