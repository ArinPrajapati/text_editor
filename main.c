/* * includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h> // for open() function
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// carrage return is the character that moves the cursor to the beginning of the
// line.

//  struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from
//  <termios.h>.

/** defines */
// #define is used to create a macro, which is a constant value that can be used
// in place of a variable.
#define CTRL_KEY(k) ((k) & 0x1f)
// the CTRL_KEY('key') macro is used to take a character and bitwise-AND it with
// 00011111, which is 31 in decimal, to get the control key value.

#define TEXT_EDITOR_VERSION "0.0.1"
#define TEXT_EDITOR_TAB_STOP 8
#define EDITOR_QUIT_TIMES 3
#define HL_HIGHLIGHT_NUMBER (1 << 0)
#define HL_HIGHLIGHT_STRING (1 << 1)
/** function prototypes **/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

enum editorKey {
  Back_Space = 127,
  Arrow_Left = 1000,
  Arrow_Right,
  Arrow_Up,
  Arrow_Down,
  Del_Key,
  HOME_KEY,
  END_KEY,
  Page_Up,
  Page_Down,
};

enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

/*** data ***/

struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
};

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
  // global struct to store the editor configuration.
} erow;
struct editorConfig {
  int cx, cy;
  // cx and cy are the x and y coordinates of the cursor.
  int rx;
  // rx is the x coordinate of the cursor in the render field.
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;

  int numrows;
  erow *row;
  int dirty;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct editorSyntax *syntax;
  struct termios orig_termios;
};

struct editorConfig E;

//** filetypes ** //

char *C_HL_extensions[] = {".c", ".h", ".cpp", ".js", ".ts", NULL};
char *C_HL_keyword[] = {"switch",    "if",      "while",   "for",    "break",
                        "continue",  "return",  "else",    "struct", "union",
                        "typedef",   "static",  "enum",    "class",  "case",
                        "int|",      "long|",   "double|", "float|", "char|",
                        "unsigned|", "signed|", "void|",   NULL};
// HLDB stand for highlight database , our editorSytax struct for C language
// contains the string 'C' for filetype field, the extension ".c" , ".h" ,
// ".cpp",".js" , ".ts" why js and ts because it is fun, what are you gonna do
struct editorSyntax HLDB[] = {
    {"c", C_HL_extensions, C_HL_keyword, "//","/*","*/",
     HL_HIGHLIGHT_NUMBER | HL_HIGHLIGHT_STRING},
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  // perror() comes from <stdio.h> and is used to print an error message to the
  // terminal.
  exit(1);
  // exit() comes from <stdlib.h> and is used to exit the program.
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
    die("tcsetattr");
  };
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");
  // atexit() comes from <stdlib.h> and is used to register a function to be
  // called when the program exits.
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
  // OPOST is a bitflag that disables output processing, which converts newlines
  // to carriage returns and linefeeds.

  raw.c_oflag &= ~(CS8);

  // .c_oflag comes from <termios.h>
  // .c_oflag is a bitflag that contains various flags for the terminal.
  // CS8 comes from <termios.h>
  // CS8 is not a flag, it is a bit mask with multiple bits, which we set using
  // the bitwise-OR (|) operator unlike all the flags we are turning off. It
  // sets the character size (CS) to 8 bits per byte.on most system is already
  // set that way.

  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  // .c_lflag comes from <termios.h>
  // .c_lflag is a bitflag that contains various flags for the terminal.
  // ICANON & ISIG & IEXTEN comes from <termios.h>
  // ICANNO is a bitflag that enables canonical mode.
  // ISIG is a bitflag that allows the program to receive signals like ctrl-c
  // and ctrl-z. IEXTEN is a bitflag that disables ctrl-v. canonical mode is the
  // mode where the input is line by line, and the input is only sent to the
  // program after the user presses enter.

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  // .c_cc comes from <termios.h>
  // .c_cc is an array of bytes that control various terminal settings.
  // VMIN and VTIME are indices into the c_cc array that control reading.

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

int editorReadKey() {
  // editorReadKey() is to read a single keypress from the user and return it.

  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return Del_Key;
          case '4':
            return END_KEY;
          case '5':
            return Page_Up;
          case '6':
            return Page_Down;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return Arrow_Up;
        case 'B':
          return Arrow_Down;
        case 'C':
          return Arrow_Right;
        case 'D':
          return Arrow_Left;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {

  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    // this loop is to read the response from the terminal.
    // the response is in the form of \x1b[24;80R
    // the 24 is the row number and the 80 is the column number.
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      // read() is used to read from the terminal.
      break;
    }
    if (buf[i] == 'R') {
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

int getWindowsSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
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
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/** syntax highlighting **/

// the is_separator function that takes a character and returns true if it's
// considered a separator character
int is_separator(int c) {
  return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
  // strchr() comes from <string.h> . It looks for the first occurrence of  a
  // character in a string.
}

void editorUpdateSyntax(erow *row) {
  row->hl = realloc(row->hl, row->rsize);
  memset(row->hl, HL_NORMAL, row->rsize);
  // memset() comes from <string.h>
  if (E.syntax == NULL)
    return;

  char **keywords = E.syntax->keywords;

  char *scs = E.syntax->singleline_comment_start;
  char *mcs = E.syntax->multiline_comment_start;
  char *mce = E.syntax->multiline_comment_end;
  int scs_len = scs ? strlen(scs) : 0;
  int mcs_len = mcs? strlen(mcs):0;
  int mce_len = mce ? strlen(mce):0;
  int prev_step = 1;
  int in_string = 0; // false
  int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
  int i = 0;
  while (i < row->rsize) {
    char c = row->render[i];
    unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

    if (scs_len && !in_string && !in_comment) {
      if (!strncmp(&row->render[i], scs, scs_len)) {
        memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        break;
      }
    }

    if(mcs_len && mce_len && !in_string){
      if(in_comment) {
       row->hl[i] = HL_MLCOMMENT;
      if(!strncmp(&row->render[i],mce,mce_len)){
           memset(&row->hl[i],HL_MLCOMMENT,mce_len);
          i += mce_len;
          in_comment =0 ;
          prev_step = 1;
          continue;
        }else{
          i++;
          continue;
        }
      }else if (!strncmp(&row->render[i],mcs,mcs_len)){
        memset(&row->hl[i],HL_MLCOMMENT,mcs_len);
        i += mcs_len;
        in_comment = 1;
        continue;
      }

    }



    if (E.syntax->flags & HL_HIGHLIGHT_STRING) {
      if (in_string) {
        row->hl[i] = HL_STRING;
        if (c == '\\' && i + 1 < row->rsize) {
          row->hl[i + 1] = HL_STRING;
          i += 2;
          continue;
        }
        if (c == in_string)
          in_string = 0;
        i++;
        prev_step = 1;
        continue;
      } else {
        if (c == '"' || c == '\'') {
          in_string = c;
          row->hl[i] = HL_STRING;
          i++;
          continue;
        }
      }
    }
    if (E.syntax->flags & HL_HIGHLIGHT_NUMBER) {
      if ((isdigit(c) && (prev_step || prev_hl == HL_NORMAL)) ||
          (c == '.' && prev_hl == HL_NUMBER)) {
        row->hl[i] = HL_NUMBER;
        i++;
        prev_step = 0;
        continue;
      }
    }

    if (prev_step) {
      int j;
      for (j = 0; keywords[j]; j++) {
        int klen = strlen(keywords[j]);
        int kw2 = keywords[j][klen - 1] == '|';
        if (kw2)
          klen--;

        if (!strncmp(&row->render[i], keywords[j], klen) &&
            is_separator(row->render[i + klen])) {
          memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          i += klen;
          break;
        }
      }
      if (keywords[j] != NULL) {
        prev_step = 0;
        continue;
      }
    }
    prev_step = is_separator(c);
    i++;
  }

  int changed = (row->hl_open_comment != in_comment);
  row->hl_open_comment = in_comment;
  if(changed && row->idx + 1 < E.numrows) editorUpdateSyntax(&E.row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
  switch (hl) {
  
  case HL_COMMENT:
  case HL_MLCOMMENT:
    return 36;
  case HL_KEYWORD1:
    return 33;
  case HL_KEYWORD2:
    return 32;
  case HL_STRING:
    return 35;
  case HL_NUMBER:
    return 31;
  case HL_MATCH:
    return 34;
  default:
    return 37;
  }
}

void editorSelectSyntaxHighlight() {
  E.syntax = NULL;
  if (E.filename == NULL)
    return;
  // strrchr() and strcmp() come from <string.h>. strrchr() returns a pointer to
  // the last occurrence of a character in a string, and strcmp() returns 0 if
  // two given strings are equal.
  char *ext = strrchr(E.filename, '.');

  for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    struct editorSyntax *s = &HLDB[j];
    unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;

        int filerow;
        for (filerow = 0; filerow < E.numrows; filerow++) {
          editorUpdateSyntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
  }
}
/** row operations */

int editorRowsCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (TEXT_EDITOR_TAB_STOP - 1) - (rx % TEXT_EDITOR_TAB_STOP);
    rx++;
  }

  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;

  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (TEXT_EDITOR_TAB_STOP - 1) - (cur_rx % TEXT_EDITOR_TAB_STOP);

    if (cur_rx > rx)
      return cx;
  }
  return cx;
}

void editorUpdateRow(erow *row) {

  int tabs = 0;

  int j;

  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);
  row->render = malloc(row->size + tabs * (TEXT_EDITOR_TAB_STOP - 1) + 1);

  int idx = 0;

  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % (TEXT_EDITOR_TAB_STOP - 1) != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;

  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {

  if (at < 0 || at > E.numrows)
    return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for(int j = at + 1;j <= E.numrows;j++) E.row[j].idx++;  
  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  if (E.row[at].chars == NULL)
    die("malloc");

  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
  // free the memory
}

void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) {
    return;
  }
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  for(int j = at;j < E.numrows - 1;j++) E.row[j].idx--;
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) {
    at = row->size;
  }

  row->chars = realloc(row->chars, row->size + 2);
  // the realloc() function is used to allocate memory.
  // the row->chars is the buffer.
  // the row->size is the length of the buffer.

  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  // the memmove() function is used to copy memory from one location to another.
  // the &row->chars[at + 1] is the destination.
  // the &row->chars[at] is the source.
  // the row->size - at + 1 is the number of bytes to copy.
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  // we use realloc() to allocate memory for the new string. which is the old
  // size + the length of the new string + 1 for the null terminator.
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  // the memmove() function is used to copy memory from one location to another.
  // we are overwriting the character at the cursor with the character after it.
  // and then we are moving the rest of the characters to the left.
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

/*** editor operations */

void editorInsertChars(int c) {
  if (E.cy == E.numrows) {
    // the E.cy is the y coordinate of the cursor.
    // the E.numrows is the number of rows.
    // if both are equal, we append a new row.
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    // the '\0' character is used to terminate the string.
    editorUpdateRow(row);
  }

  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.numrows)
    return;
  if (E.cx == 0 && E.cy == 0) {
    return;
  }
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}
/*** file i/o */

char *editorRowsString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++) {
    totlen += E.row[j].size + 1;
  }
  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;

  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();
  // strdup() is used to duplicate a string.
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    die("fopen");
  }
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
      linelen--;
    }
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {

  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }

  int len;
  char *buf = editorRowsString(&len);

  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);

  // open is a system call that opens a file.
  // the O_RDWR flag is used to open the file for reading and writing.
  // the O_CREAT flag is used to create the file if it does not exist.
  // the 0644 is the file permissions.
  // which is read and write for the user, and read for the group and others.

  if (fd != -1) {
    // ftruncate() is used to truncate a file to a specified length.
    // the fd is the file descriptor.
    // len is the length of the file.
    if (ftruncate(fd, len) != 1) {
      // write() is used to write to a file.
      // the fd is the file descriptor.
      // the buf is the buffer of data.
      // the len is the length of the data.

      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

// find

void editorFindCallBack(char *query, int key) {
  static int last_match = -1;
  // static is used to make a variable persist between function calls.
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;

  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }
  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;

    return;
  } else if (key == Arrow_Right || key == Arrow_Down) {
    direction = 1;
  } else if (key == Arrow_Left || key == Arrow_Up) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }

  if (last_match == -1)
    direction = 1;

  int current = last_match;

  for (int i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1)
      current = E.numrows - 1;
    else if (current == E.numrows)
      current = 0;
    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render);
      E.rowoff = E.numrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}
void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_colooff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query =
      editorPrompt("Search: %s (Use ESC to cancel | Arrows to navigate ) ",
                   editorFindCallBack);
  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_colooff;
    E.rowoff = saved_rowoff;
  }
}

//** append buff */

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

// {NULL, 0} is the initializer for the abuf struct.
// the abuf struct is used to store the buffer and the length of the buffer.

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  // the realloc() function is used to allocate memory.
  // the ab->b is the buffer.
  // the ab->len is the length of the buffer.
  // the len is the length of the string.

  if (new == NULL)
    return;

  memcpy(&new[ab->len], s, len);
  // the memcpy() function is used to copy memory from one location to another.
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/** input ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  // size_t is an unsigned integer type that is used to represent the size of
  // objects in bytes.
  char *buf = malloc(bufsize);

  size_t buflen = 0;
  buf[0] = '\0';

  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();

    int c = editorReadKey();
    if (c == Del_Key || c == CTRL_KEY('h') || c == Back_Space) {
      if (buflen != 0)
        buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback)
        callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      // which is enter.
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback)
          callback(buf, c);

        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }

    if (callback)
      callback(buf, c);
  }
}

void editorSetStatusMessage(const char *fmt, ...) {
  // va_list, va_start(), vsnprintf(), and va_end() all come from <stdarg.h>.
  // va_list is a type to hold information about variable arguments.
  // va_start() is a macro to initialize the va_list.
  // vsnprintf() is a function to format a string.

  va_list ap;

  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
  case Arrow_Left:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) {
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case Arrow_Right:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row && E.cx == row->size) {
      E.cy++;
      E.cx = 0;
    }
    break;
  case Arrow_Up:
    if (E.cy != 0) {
      E.cy--;
    }
    break;

  case Arrow_Down:
    if (E.cy < E.numrows - 1) {
      E.cy++;
    }
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;

  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {

  //  editorProcessKeypress() is to process the keypresses that the editor
  //  reads.

  static int quit_times = EDITOR_QUIT_TIMES;
  int c = editorReadKey();

  switch (c) {

  case '\r':
    // the '\r' character is the carriage return character that is "Enter".
    editorInsertNewline();
    break;

  case CTRL_KEY('q'):

    if (E.dirty && quit_times > 0) {
      editorSetStatusMessage("WARNING!!! File has unsaved changes. Press "
                             "Ctrl-Q %d more times to quit.",
                             quit_times);
      quit_times--;
      return;
    }
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case CTRL_KEY('s'):
    editorSave();
    break;

  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    if (E.cy < E.numrows)
      E.cx = E.row[E.cy].size;
    break;

  case CTRL_KEY('f'):
    editorFind();
    break;

  case Back_Space:
  case CTRL_KEY('h'): // the 'h' character is the backspace character.
  case Del_Key:
    if (c == Del_Key)
      editorMoveCursor(Arrow_Right);
    editorDelChar();
    break;

  case Page_Up:
  case Page_Down: {
    if (c == Page_Up) {
      E.cy = E.rowoff;
    } else if (c == Page_Down) {
      E.cy = E.rowoff + E.screenrows - 1;
      if (E.cy > E.numrows)
        E.cy = E.numrows;
    }
    int times = E.screenrows;
    while (times--) {
      editorMoveCursor(c == Page_Up ? Arrow_Up : Arrow_Down);
    }
  } break;

  case Arrow_Down:
  case Arrow_Up:
  case Arrow_Left:
  case Arrow_Right:
    editorMoveCursor(c);
    break;

  case CTRL_KEY('l'):
  case '\x1b': // this is the escape character.
    break;

  default:
    editorInsertChars(c);
    break;
  }

  quit_times = EDITOR_QUIT_TIMES;
}

/** output */

void editorScroll() {
  E.rx = 0;

  if (E.cy < E.numrows) {
    E.rx = editorRowsCxToRx(&E.row[E.cy], E.cx);
  }

  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }

  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {

  // this loop is to draw the rows of tildes.
  int y;

  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;

    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomeLen =
            snprintf(welcome, sizeof(welcome), "Text Editor -- version %s",
                     TEXT_EDITOR_VERSION);
        if (welcomeLen > E.screencols) {
          welcomeLen = E.screencols;
        }

        int padding = (E.screencols - welcomeLen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomeLen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0)
        len = 0;
      if (len > E.screencols)
        len = E.screencols;
      char *c = &E.row[filerow].render[E.coloff];
      int current_color = -1;
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      for (int j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);
          if(current_color != -1){
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm",current_color);
            abAppend(ab,buf,clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }
    abAppend(ab, "\x1b[K", 3);
    // the 'K' command is used to clear the line.
    abAppend(ab, "\r\n", 2);
  }
  // this if statement is to check if we are on the last row.
  // if we are not on the last row, we print a newline character.
}
void editorDrawStatusBar(struct abuf *ab) {

  // this function is to draw the status bar.
  // 7m is the command to invert the colors.
  abAppend(ab, "\x1b[7m", 4);
  char status[90], rstatus[90]; /// this number is the length of the status bar.
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     E.filename ? E.filename : "[No Name]", E.numrows,
                     E.dirty ? "(modified)" : "");
  int rlen =
      snprintf(rstatus, sizeof(rstatus), "%s |  %d/%d",
               E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);
  if (len > E.screencols)
    len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }

  abAppend(ab, "\x1b[m", 3);
  // the 'm' command is used to turn off the color inversion.
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);

  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5) {
    abAppend(ab, E.statusmsg, msglen);
  }
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  // the 'l' command is used to hide the cursor.
  // the \x1b is the escape character. this is to hide the cursor.
  // abAppend(&ab, "\x1b[2J", 4);
  // '2J' command is used to clear the screen.
  // the \x1b is the escape character.
  // This escape sequence is only 4 bytes long, and uses the J command (Erase In
  // Display) to clear the screen. 'H' command is used to position the cursor.
  // \x1b is the escape character.
  // This escape sequence is only 3 bytes long, and uses the H command (Cursor
  // Position) to position the cursor.

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  // the snprintf() function is used to print a formatted string to a buffer.
  // the \x1b is the escape character, and the [ is the left bracket character.
  // the %d is a placeholder for a number.
  // the ; is the semicolon character.
  // the H is the command to position the cursor.
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  // \x1b is the escape character.
  // This escape sequence is only 3 bytes long, and uses the H command (Cursor
  // Position) to position the cursor. The H command actually takes two
  // arguments: the row number and the column number at which to position the
  // cursor.

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/** init */

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;

  if (getWindowsSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");

  E.screenrows -= 2;
}

// initEditor()’s job will be to initialize all the fields in the E struct.

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };
  return 0;
}
