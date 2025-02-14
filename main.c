/*** includes ***/

#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

// carrage return is the character that moves the cursor to the beginning of the line.

//  struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from <termios.h>.

/** defines */
// #define is used to create a macro, which is a constant value that can be used in place of a variable.
#define CTRL_KEY(k) ((k) & 0x1f)
// the CTRL_KEY('key') macro is used to take a character and bitwise-AND it with 00011111, which is 31 in decimal, to get the control key value.

/*** data ***/

// global struct to store the editor configuration.
struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;
/*** terminal ***/

void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    // perror() comes from <stdio.h> and is used to print an error message to the terminal.
    exit(1);
    // exit() comes from <stdlib.h> and is used to exit the program.
}

void disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    {
        die("tcsetattr");
    };
}

void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    // atexit() comes from <stdlib.h> and is used to register a function to be called when the program exits.
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;

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

int getCursorPosition(int *rows, int *cols)
{

    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1)
    {
        // this loop is to read the response from the terminal.
        // the response is in the form of \x1b[24;80R
        // the 24 is the row number and the 80 is the column number.
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
        {
            // read() is used to read from the terminal.
            break;
        }
        if (buf[i] == 'R')
        {
            break;
        }
        i++;
    }

    buf[i] = '\0';
    // the \0 character is used to terminate the string.

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;

    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    // this is to parse the row and column numbers from the response.
    // sscanf() is used to parse the response.

    return 0;
}

int getWindowsSize(int *rows, int *cols)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[99C\x1b[999B", 12) != 12)
            return -1;
        // the write() function is used to write to the terminal.
        // STDOUT_FILENO is a file descriptor that represents standard output.
        // \x1b is the escape character.
        // [99C is the escape sequence to move the cursor 99 columns to the right.]
        // [999B is the escape sequence to move the cursor 999 rows down.]
        return getCursorPosition(rows, cols);
    }
    // ioctl() , TIOCGWINSZ, winsize all come from <sys/ioctl.h>
    // the ioctl() function is used to get the size of the terminal.
    // TIOCGWINSZ is a command that gets the window size.
    // ws.ws_col is the number of columns in the terminal.
    // ws.ws_row is the number of rows in the terminal.
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/** input ***/

void editorProcessKeypress()
{
    //  editorProcessKeypress() is to process the keypresses that the editor reads.

    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}

/** output */
void editorDrawRows()
{

    // this loop is to draw the rows of tildes.
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // \x1b is the escape character.
    // there is 4 in the end because the escape sequence is 4 bytes long.
    // the write() function is used to write to the terminal.
    // STDOUT_FILENO is a file descriptor that represents standard output.
    write(STDOUT_FILENO, "\x1b[H", 3);
    // \x1b is the escape character.
    // This escape sequence is only 3 bytes long, and uses the H command (Cursor Position) to position the cursor. The H command actually takes two arguments: the row number and the column number at which to position the cursor.

    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/** init */

void initEditor()
{
    if (getWindowsSize(&E.screenrows, &E.screencols) == -1)
        die("getWindowSize");
}

// initEditor()’s job will be to initialize all the fields in the E struct.

int main()
{
    enableRawMode();
    initEditor();
    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    };
    return 0;
}