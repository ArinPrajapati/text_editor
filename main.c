#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

//  struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from <termios.h>.

struct termios orig_termios;

void disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode()
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    // atexit() comes from <stdlib.h> and is used to register a function to be called when the program exits.
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);

    // ICANON & ISIG comes from <termios.h> 
    // ICANNO is a bitflag that enables canonical mode.
    // ISIG is a bitflag that allows the program to receive signals like ctrl-c and ctrl-z.
    // canonical mode is the mode where the input is line by line, and the input is only sent to the program after the user presses enter.

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main()
{
    char c;
    enableRawMode();
    // read() and STDIN_FILENO comes from  usinstd.h ,
    // the line below read bytes from standard input into the varibale c , and keep doing till there is no bytes to read
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
    {
        // iscrtl() comes from <ctype.h> and checks if the character is a control character.
        // control characters are characters that are not printable, like backspace, enter, and escape.
        if (iscntrl(c))
        {
            // for control characters, we print the ascii value of the character in the format of ^<char>.
            printf("%d\n", c);
        }
        else
        {
            // printf() comes from <stdio.h> and is used to print formatted output to the terminal.
            // %d tell it to format the byte as a decimal number, and %c tells it to format the byte as a character.
            printf("%d ('%c')\n", c, c);
        }
    };
    return 0;
}