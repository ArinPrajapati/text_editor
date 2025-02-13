/*** includes ***/

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

// carrage return is the character that moves the cursor to the beginning of the line.

//  struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from <termios.h>.

/** defines */
// #define is used to create a macro, which is a constant value that can be used in place of a variable.
#define CTRL_KEY(k) ((k) & 0x1f)
// the CTRL_KEY('key') macro is used to take a character and bitwise-AND it with 00011111, which is 31 in decimal, to get the control key value.

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

char editorReadKey()
{
    // editorReadKey() is to read a single keypress from the user and return it.

    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1) != 1))
    {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    return c;
}

/** input ***/

void editorProcessKeypress()
{
    //  editorProcessKeypress() is to process the keypresses that the editor reads.

    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        exit(0);
        break;
    }
}

/** init */
int main()
{
    enableRawMode();

    while (1)
    {
        editorProcessKeypress();
    };
    return 0;
}