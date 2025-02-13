/*** includes ***/

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

// carrage return is the character that moves the cursor to the beginning of the line.

//  struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from <termios.h>.

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s)
{
    perror(s);
    // perror() comes from <stdio.h> and is used to print an error message to the terminal.
    exit(1);
    // exit() comes from <stdlib.h> and is used to exit the program.
}

void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    {
        die("tcsetattr");
    };
}

void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");
    // atexit() comes from <stdlib.h> and is used to register a function to be called when the program exits.
    atexit(disableRawMode);
    struct termios raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    // BRKINT & ICRNL & INPCK & ISTRIP & IXON comes from <termios.h>
    // BRKINT is a bitflag that disables break conditions.
    // ICRNL is a bitflag that disables carriage return.
    // INPCK is a bitflag that disables parity checking.
    // ISTRIP is a bitflag that disables stripping of the 8th bit.
    // IXON is a bitflag that disables ctrl-s and ctrl-q.

    raw.c_oflag &= ~(OPOST);

    // OPOST comes from <termios.h>
    // OPOST is a bitflag that disables output processing, which converts newlines to carriage returns and linefeeds.

    raw.c_oflag &= ~(CS8);

    // .c_oflag comes from <termios.h>
    // .c_oflag is a bitflag that contains various flags for the terminal.
    // CS8 comes from <termios.h>
    // CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we are turning off. It sets the character size (CS) to 8 bits per byte.on most system is already set that way.

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // .c_lflag comes from <termios.h>
    // .c_lflag is a bitflag that contains various flags for the terminal.
    // ICANON & ISIG & IEXTEN comes from <termios.h>
    // ICANNO is a bitflag that enables canonical mode.
    // ISIG is a bitflag that allows the program to receive signals like ctrl-c and ctrl-z.
    // IEXTEN is a bitflag that disables ctrl-v.
    // canonical mode is the mode where the input is line by line, and the input is only sent to the program after the user presses enter.

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    // .c_cc comes from <termios.h>
    // .c_cc is an array of bytes that control various terminal settings.
    // VMIN and VTIME are indices into the c_cc array that control reading.

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

/** init */
int main()
{
    enableRawMode();

    while (1)
    {
        char c = '\0';
        // read() and STDIN_FILENO comes from  usinstd.h ,
        // the line below read bytes from standard input into the varibale c , and keep doing till there is no bytes to read
        // read() comes from <unistd.h> and is used to read bytes from a file descriptor.
        // errno comes from <errno.h> and is used to store the error code.
        // EAGAIN comes from <errno.h> and is the error code for "resource temporarily unavailable".

        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");
        // iscrtl() comes from <ctype.h>
        // iscrtl() checks if the character is a control character.
        // control characters are characters that are not printable, like backspace, enter, and escape.
        if (iscntrl(c))
        {
            // for control characters, we print the ascii value of the character in the format of ^<char>.
            // for example, ^C is ctrl-c.
            // \r is carriage return, and \n is newline.
            printf("%d\r\n", c);
        }
        else
        {
            // printf() comes from <stdio.h> and is used to print formatted output to the terminal.
            // %d tell it to format the byte as a decimal number, and %c tells it to format the byte as a character.
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q')
            break;
    };
    return 0;
}