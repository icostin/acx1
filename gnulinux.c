#ifndef _WIN32

#define _POSIX_C_SOURCE 199309

/* acx1 - Application Console Interface - ver. 1
 *
 * GNU/Linux Terminal support
 *
 * Changelog:
 *  - 2013/01/30 Costin Ionescu: fixed bug when filling EOL
 *  - 2013/01/27 Costin Ionescu:
 *    - acx1_rect()
 *  - 2013/01/19 Costin Ionescu: 
 *    - tty_write(): wait for tty to be ready when write() fails with EAGAIN
 *    - tty_write(): fixed bug when passing remaining buffer after EINTR
 *  - 2013/01/06 Costin Ionescu: initial release
 *
 */
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>
#include <pthread.h>

#include "acx1.h"

char * escstr (char * out, size_t out_len, void const * buf, size_t len);

//static char const S7C1T[] = "\e F";
// static char const S8C1T[] = "\e G";
char const SET_ANSI_CONFORMANCE_LEVEL_1[] = "\e L";
char const SET_ANSI_CONFORMANCE_LEVEL_2[] = "\e M";
char const SET_ANSI_CONFORMANCE_LEVEL_3[] = "\e N";
char const REPORT_CURSOR_POSITION[] = "\e[6n";
char const APPLICATION_KEYPAD[] = "\e=";
char const NORMAL_KEYPAD[] = "\e>";
char const CLEAR_ALL_SCREEN[] = "\e[2J";
char const HIDE_CURSOR[] = "\e[?25l";
char const SHOW_CURSOR[] = "\e[?25h";
char const WRAPAROUND_MODE[] = "\e[?7h";
char const NO_WRAPAROUND_MODE[] = "\e[?7l";
char const BACKARROW_SENDS_DEL[] = "\e[?67h";

static pthread_t worker_th;
static pthread_mutex_t mutex;
static pthread_cond_t event_cond;
static pthread_cond_t read_event_done_cond;
static pthread_cond_t cursor_cond;
static FILE * log_file = NULL;
static int log_level = 0;
static int tty_fd = -1;
static struct termios orig_tio;
static uint16_t screen_width, screen_height;
static uint16_t user_row, user_col;
static uint16_t real_row, real_col;
static int worker_pipe[2] = { -1, -1};
static char writing = 0;
static char tio_set = 0;
static char th_created = 0;
static char mutex_created = 0;
static char event_cond_created = 0;
static char cursor_cond_created = 0;
static char waiting_for_event = 0;
static char waiting_for_cursor = 0;
static char read_event_done = 0;
static char read_event_done_created = 0;
static char screen_resized = 0;
static char sigwinch_set = 0;
static char finishing = 0;
static uint8_t cursor_mode = 0;

static uint32_t * queue_a = NULL;
static uint8_t queue_shift; // log2 size of queue
static unsigned int queue_len, qx_mask, qx_begin, qx_end;
static unsigned int worker_error = 0;
static unsigned int decode_mode = 0;

#define LW(...) (log_level >= 2 && log_file ? \
                 (fprintf(log_file, "[acx1]Warning(%s:%03u:%s): ", \
                                    __FILE__, __LINE__, __FUNCTION__), \
                  fprintf(log_file, __VA_ARGS__), fflush(log_file)) : 0)

#define LI(...) (log_level >= 3 && log_file ? \
                 (fprintf(log_file, "[acx1]Info(%s:%03u:%s): ", \
                                   __FILE__, __LINE__, __FUNCTION__), \
                  fprintf(log_file, __VA_ARGS__), fflush(log_file)) : 0)

#define LE(...) (log_level >= 1 && log_file ? \
                 (fprintf(log_file, "[acx1]Error(%s:%03u:%s): ", \
                                   __FILE__, __LINE__, __FUNCTION__), \
                  fprintf(log_file, __VA_ARGS__), fflush(log_file)) : 0)

#define Z(_cond, _rc) \
  if ((_cond)) { \
    LE("(%s) failed, status = %s (%u)\n", #_cond, acx1_status_str(_rc), _rc); \
    rc = _rc; goto l_fail; \
  } else ((void) 0)

#define C(_cond, _rc) Z(!(_cond), _rc)

/* acx1_name ****************************************************************/
ACX1_API char const * ACX1_CALL acx1_name ()
{
  return "acx1-gnu_linux";
}

/* acx1_set_log_file ********************************************************/
ACX1_API void acx1_logging (int level, FILE * lf)
{
  log_file = lf;
  log_level = level;
}

/* set_cursor_pos_str ********************************************************/
static uint_t ACX1_CALL set_cursor_pos_str (uint8_t * buf, 
                                            uint16_t r, uint16_t c)
{
  return sprintf((char *) buf, "\e[%u;%uH", r, c);
}

/* set_attr_str *************************************************************/
static uint_t ACX1_CALL set_attr_str (uint8_t * buf, int bg, int fg, int mode)
{
  int ia[0x10], in;
  int o, k;

  o = 0;
  in = 0;
  ia[in++] = 0;
  if (bg < 0) return ACX1_NO_CODE;
  if (bg < 8) ia[in++] = 40 + bg;
  else if (bg < 0x10) ia[in++] = 100 - 8 + bg;
  else { ia[in++] = 38; ia[in++] = 5; ia[in++] = bg; }

  if (fg < 0) return ACX1_NO_CODE;
  if (fg < 8) ia[in++] = 30 + fg;
  else if (fg < 0x10) ia[in++] = 90 - 8 + fg;
  else { ia[in++] = 48; ia[in++] = 5; ia[in++] = fg; }

  if ((mode & ACX1_BOLD)) ia[in++] = 1;
  if ((mode & ACX1_UNDERLINE)) ia[in++] = 4;
  if ((mode & ACX1_INVERSE)) ia[in++] = 7;

  buf[0] = 0x1B;
  buf[1] = '[';
  o = 2;
  for (k = 0; k < in; ++k, o += strlen((char *) &buf[o]))
  {
    sprintf((char *) &buf[o], "%u;", ia[k]);
  }
  buf[o - 1] = 'm';
  return o;
}

/* qpush1 *******************************************************************/
static unsigned int qpush1 (uint32_t v)
{
  unsigned int rc;
  // pthread_mutex_lock(mutex);
  if (queue_len == qx_mask)
  {
    rc = ACX1_NO_CODE;
    goto l_exit;
  }
  queue_a[qx_end] = v;
  qx_end = (qx_end + 1) & qx_mask;
  queue_len += 1;
  rc = 0;
l_exit:
  // pthread_mutex_unlock(mutex);
  return rc;
}

/* qpop1 ********************************************************************/
static unsigned int qpop1 ()
{
  unsigned int v;
  // pthread_mutex_lock(mutex);
  if (!queue_len) v = 0;
  else
  {
    v = queue_a[qx_begin];
    qx_begin = (qx_begin + 1) & qx_mask;
    queue_len -= 1;
  }
  // pthread_mutex_unlock(mutex);
  return v;
}

/* tty_write ****************************************************************/
static int tty_write (void const * data, size_t len)
{
  ssize_t wlen;
  uint8_t const * p;
  int e;
  fd_set fds;

  if (log_level > 3)
  {
    uint8_t tmp[0x100];
    uint_t tl;
    tl = (len > sizeof(tmp) / 2) ? sizeof(tmp) / 2 : len;
    LI("tty_write: tty=%d len=0x%lX %s\n", tty_fd, (long) len, (char *) acx1_hexz(tmp, data, tl));
  }

  for (p = data; len; )
  {
    wlen = write(tty_fd, p, len);
    if (wlen < 0)
    {
      e = errno;
      if (e == EINTR) continue;
      LE("write error %d = %s\n", e, strerror(e));
      FD_ZERO(&fds);
      FD_SET(tty_fd, &fds);
      e = select(tty_fd + 1, NULL, &fds, NULL, NULL);
      LI("waiting for write to be available for tty_fd %d\n", tty_fd);
      wlen = 0;
    }
    p += wlen;
    len -= wlen;
  }
  return len ? -1 : 0;
}

#define tty_write_const(_m) (tty_write(_m, sizeof(_m) - 1))

/* winch_signal *************************************************************/
static void winch_signal (int sig, siginfo_t * si, void * unused)
{
  ssize_t z;
  int e;

  (void) sig;
  (void) si;
  (void) unused;

  LI("received SIGWINCH!\n");

  if (worker_pipe[1] < 0)
  {
    LW("worker_pipe not inited!\n");
    return;
  }
  for (;;)
  {
    z = write(worker_pipe[1], "z", 1);
    if (z < 0)
    {
      e = errno;
      if (e == EINTR) continue;
      LW("writing to worker_pipe failed (error %d = %s)\n", e, strerror(e));
      break;
    }
    if (z == 0)
    {
      LW("writing to worker_pipe returned 0!\n");
      break;
    }
    LI("wrote resize notification to worker_pipe\n");
    break;
  }
}

/* parse_ints ***************************************************************/
static int parse_ints (uint8_t * data, size_t len, uint32_t * out,
                       unsigned int out_max_len, unsigned int * out_len)
{
  uint8_t b;
  int g;
  size_t i;

  *out_len = 0;
  for (i = 0, g = 1; i < len; ++i)
  {
    b = data[i];
    if (b >= '0' && b <= '9')
    {
      if (g)
      {
        if (*out_len == out_max_len) return i;
        *out_len += 1;
        *out = 0;
        g = 0;
      }

      *out = *out * 10 + b - '0';
    }
    else if (b == ';')
    {
      out++;
      g = 1;
    }
    else break;
  }
  return i;
}

/* decode_modifiers_code ****************************************************/
static int32_t decode_modifiers_code (int c)
{
  switch (c)
  {
  case 0: return 0; //
  case 2: return ACX1_SHIFT;
  case 3: return ACX1_ALT;
  case 4: return ACX1_SHIFT | ACX1_ALT;
  case 5: return ACX1_CTRL;
  case 6: return ACX1_SHIFT | ACX1_CTRL;
  case 7: return ACX1_ALT | ACX1_CTRL;
  case 8: return ACX1_SHIFT | ACX1_ALT | ACX1_CTRL;
  }
  return -1;
}

#define DI_KEY 0
#define DI_MORE 1 // need more data
#define DI_BAD 2
#define DI_ESC 3

#define REPORT_CURSOR_POSITION_REPLY 1

/* decode_input *************************************************************/
static int decode_input (uint8_t * data, size_t len, uint32_t * out,
                         size_t * used_len_p, int mode)
{
  uint8_t b;
  int i, m;
  unsigned int nl;
  uint32_t n[0x10];
  //size_t i;
  if (len == 0) return DI_MORE;
  b = data[0];
  if (b >= 0x20 && b <= 0x7E)
  {
    *out = b;
    *used_len_p = 1;
    return DI_KEY;
  }
  if (b >= 0xC0)
  {
    // decode utf8 char >= 0x80
    int l;
    l = acx1_utf8_char_decode_strict(data, len, out);
    if (l < 0) return DI_BAD;
    *used_len_p = l;
    return DI_KEY;
  }
  if (b >= 0x80)
  {
    // term escapes
    *out = 'E';
    *used_len_p = 1;
    return DI_KEY;
  }

  if (b == 0x1B)
  {
    if (len == 1)
    {
      *out = ACX1_ESC;
      *used_len_p = 1;
      return DI_KEY;
    }
    b = data[1];
    if (b == 0x1B)
    {
      if (mode == 1) return DI_BAD;
      i = decode_input(data + 1, len - 1, out, used_len_p, 1);
      if (i != DI_KEY) return DI_BAD;
      *out |= ACX1_ALT;
      *used_len_p += 1;
      return DI_KEY;
    }
    data += 2; len -= 2;
    if (b == '[' && len) // CSI
    {
      if (data[0] == '[')
      {
        /* linux text-mode terminal: */
        if (len != 2) return DI_BAD;
        switch (data[1])
        {
        case 'A': case 'B': case 'C': case 'D': case 'E':
          *out = ACX1_F1 + data[1] - 'A'; return DI_KEY;
        }
        return DI_BAD;
      }
      // if (data[0] == '?')
      // {
      //   if (len == 1) return DI_BAD;
      //   i = parse_ints(data  + 1, len - 1, n,
      //                  sizeof(n) / sizeof(uint32_t), &nl);
      //   if ((size_t) i == len) return DI_BAD;
      // *used_len_p = 3 + i + 1;
      //   switch (data[i])
      //   {
      //   case 'R':
      //     if (nl != 2) return DI_BAD;
      //     out[0] = REPORT_CURSOR_POSITION_REPLY;
      //     out[1] = n[0];
      //     out[2] = n[1];
      //     return DI_ESC;
      //   }
      //   return DI_BAD;
      // }

      i = parse_ints(data, len, n, sizeof(n) / sizeof(uint32_t), &nl);
      if ((size_t) i == len) return DI_BAD;
      b = data[i];
      *used_len_p = 2 + i + 1;
      switch (b)
      {
      case '~':
        m = decode_modifiers_code(nl == 2 ? n[1] : 0);
        if (m < 0) return DI_BAD;
        if (!nl || nl > 2) return DI_BAD;
        switch (n[0])
        {
        case  1: *out = m | ACX1_HOME;          return DI_KEY;
        case  2: *out = m | ACX1_INS;           return DI_KEY;
        case  3: *out = m | ACX1_DEL;           return DI_KEY;
        case  4: *out = m | ACX1_END;           return DI_KEY;
        case  5: *out = m | ACX1_PAGE_UP;       return DI_KEY;
        case  6: *out = m | ACX1_PAGE_DOWN;     return DI_KEY;
        case  7: *out = m | ACX1_HOME;          return DI_KEY; // rxvt
        case  8: *out = m | ACX1_END;           return DI_KEY; // rxvt
        case 11: *out = m | ACX1_F1;            return DI_KEY; // rxvt
        case 12: *out = m | ACX1_F2;            return DI_KEY; // rxvt
        case 13: *out = m | ACX1_F3;            return DI_KEY; // rxvt
        case 14: *out = m | ACX1_F4;            return DI_KEY; // rxvt
        case 15: *out = m | ACX1_F5;            return DI_KEY;
        case 17: *out = m | ACX1_F6;            return DI_KEY;
        case 18: *out = m | ACX1_F7;            return DI_KEY;
        case 19: *out = m | ACX1_F8;            return DI_KEY;
        case 20: *out = m | ACX1_F9;            return DI_KEY;
        case 21: *out = m | ACX1_F10;           return DI_KEY;
        case 23: *out = m | ACX1_F11;           return DI_KEY;
        case 24: *out = m | ACX1_F12;           return DI_KEY;
        case 25: *out = ACX1_SHIFT | ACX1_F1;   return DI_KEY;
        case 26: *out = ACX1_SHIFT | ACX1_F2;   return DI_KEY;
        case 27: *out = ACX1_SHIFT | ACX1_F3;   return DI_KEY;
        case 28: *out = ACX1_SHIFT | ACX1_F4;   return DI_KEY;
        case 29: *out = ACX1_SHIFT | ACX1_F5;   return DI_KEY;
        case 31: *out = ACX1_SHIFT | ACX1_F6;   return DI_KEY;
        case 32: *out = ACX1_SHIFT | ACX1_F7;   return DI_KEY;
        case 33: *out = ACX1_SHIFT | ACX1_F8;   return DI_KEY;
        case 34: *out = ACX1_SHIFT | ACX1_F9;   return DI_KEY;
        case 35: *out = ACX1_SHIFT | ACX1_F10;  return DI_KEY;
        case 37: *out = ACX1_SHIFT | ACX1_F11;  return DI_KEY;
        case 38: *out = ACX1_SHIFT | ACX1_F12;  return DI_KEY;
        }
        return DI_BAD;
      case 'A': m = ACX1_UP;                    goto l_alt_pad;
      case 'B': m = ACX1_DOWN;                  goto l_alt_pad;
      case 'C': m = ACX1_RIGHT;                 goto l_alt_pad;
      case 'D': m = ACX1_LEFT;                  goto l_alt_pad;
      case 'E': m = ACX1_KP_CENTER;             goto l_alt_pad;
      case 'F': m = ACX1_END;                   goto l_alt_pad;
      case 'H': m = ACX1_HOME;
l_alt_pad:
        m |= decode_modifiers_code(nl == 2 ? n[1] : 0);
        if (m < 0) return DI_BAD;
        *out = m;
        return DI_KEY;
      case 'R': // CPR: report cursor position
        if (mode == 1)
        {
          if (nl != 2) return DI_BAD;
          out[0] = REPORT_CURSOR_POSITION_REPLY;
          out[1] = n[0];
          out[2] = n[1];
          return DI_ESC;
        }
       case 'P': case 'Q': case 'S':
        if (nl != 2) return DI_BAD;
        m = decode_modifiers_code(nl == 2 ? n[1] : 0);
        if (m < 0) return DI_BAD;
        *out = (ACX1_F1 + b - 'P') | m;
        return DI_KEY;
      case 'a': *out = ACX1_SHIFT | ACX1_UP;    return DI_KEY; //rxvt
      case 'b': *out = ACX1_SHIFT | ACX1_DOWN;  return DI_KEY; //rxvt
      case 'c': *out = ACX1_SHIFT | ACX1_RIGHT; return DI_KEY; //rxvt
      case 'd': *out = ACX1_SHIFT | ACX1_LEFT;  return DI_KEY; //rxvt
      case '$':
        if (nl != 1) return DI_BAD;
        m = ACX1_SHIFT;
        switch (n[0])
        {
        case 2: *out = m | ACX1_INS;            return DI_KEY; // rxvt
        case 3: *out = m | ACX1_DEL;            return DI_KEY; // rxvt
        case 5: *out = m | ACX1_PAGE_UP;        return DI_KEY; // rxvt
        case 6: *out = m | ACX1_PAGE_DOWN;      return DI_KEY; // rxvt
        case 7: *out = m | ACX1_HOME;           return DI_KEY; // rxvt
        case 8: *out = m | ACX1_END;            return DI_KEY; // rxvt
        }
        return DI_BAD;
      case '^':
        if (nl != 1) return DI_BAD;
        m = ACX1_CTRL;
        switch (n[0])
        {
        case 2: *out = m | ACX1_INS;            return DI_KEY; // rxvt
        case 3: *out = m | ACX1_DEL;            return DI_KEY; // rxvt
        case 5: *out = m | ACX1_PAGE_UP;        return DI_KEY; // rxvt
        case 6: *out = m | ACX1_PAGE_DOWN;      return DI_KEY; // rxvt
        case 7: *out = m | ACX1_HOME;           return DI_KEY; // rxvt
        case 8: *out = m | ACX1_END;            return DI_KEY; // rxvt
        }
        return DI_BAD;
      case '@':
        if (nl != 1) return DI_BAD;
        m = ACX1_CTRL | ACX1_SHIFT;
        switch (n[0])
        {
        case 2: *out = m | ACX1_INS;            return DI_KEY; // rxvt
        case 3: *out = m | ACX1_DEL;            return DI_KEY; // rxvt
        case 5: *out = m | ACX1_PAGE_UP;        return DI_KEY; // rxvt
        case 6: *out = m | ACX1_PAGE_DOWN;      return DI_KEY; // rxvt
        case 7: *out = m | ACX1_HOME;           return DI_KEY; // rxvt
        case 8: *out = m | ACX1_END;            return DI_KEY; // rxvt
        }
        return DI_BAD;
      case 'Z':
        *out = ACX1_SHIFT | ACX1_TAB; return DI_KEY;
      }
      return DI_BAD;
    }
    if (b == 'O' && len) // SS3
    {
      i = parse_ints(data, len, n, sizeof(n) / sizeof(uint32_t), &nl);
      if ((size_t) i == len) return DI_BAD;
      *used_len_p = 2 + i + 1;
      b = data[i];
      m = decode_modifiers_code(nl == 2 ? n[1] : 0);
      if (m < 0) return DI_BAD;
      switch (b)
      {
      case 'A': *out = m | ACX1_UP;                             return DI_KEY;
      case 'B': *out = m | ACX1_DOWN;                           return DI_KEY;
      case 'C': *out = m | ACX1_RIGHT;                          return DI_KEY;
      case 'D': *out = m | ACX1_LEFT;                           return DI_KEY;
      case 'E': *out = m | ACX1_KP_CENTER;                      return DI_KEY;
      case 'F': *out = m | ACX1_END;                            return DI_KEY; // evilvte
      case 'H': *out = m | ACX1_HOME;                           return DI_KEY; // evilvte
      case 'M': *out = m | ACX1_KP_ENTER;                       return DI_KEY;
      case 'j': *out = m | ACX1_KP_MUL;                         return DI_KEY;
      case 'k': *out = m | ACX1_KP_ADD;                         return DI_KEY;
      case 'l': *out = m | ACX1_KP_COMMA;                       return DI_KEY;
      case 'm': *out = m | ACX1_KP_SUB;                         return DI_KEY;
      case 'o': *out = m | ACX1_KP_DIV;                         return DI_KEY;
/* TODO: the following are not SHIFTed when in RXVT */
      case 'n': *out = ACX1_SHIFT | ACX1_KP_DEL;                return DI_KEY;
      case 'p': *out = ACX1_SHIFT | ACX1_KP_INS;                return DI_KEY;
      case 'q': *out = ACX1_SHIFT | ACX1_KP_END;                return DI_KEY;
      case 'r': *out = ACX1_SHIFT | ACX1_KP_DOWN;               return DI_KEY;
      case 's': *out = ACX1_SHIFT | ACX1_KP_PAGE_DOWN;          return DI_KEY;
      case 't': *out = ACX1_SHIFT | ACX1_KP_LEFT;               return DI_KEY;
      case 'u': *out = ACX1_SHIFT | ACX1_KP_CENTER;             return DI_KEY;
      case 'v': *out = ACX1_SHIFT | ACX1_KP_RIGHT;              return DI_KEY;
      case 'w': *out = ACX1_SHIFT | ACX1_KP_HOME;               return DI_KEY;
      case 'x': *out = ACX1_SHIFT | ACX1_KP_UP;                 return DI_KEY;
      case 'y': *out = ACX1_SHIFT | ACX1_KP_PAGE_UP;            return DI_KEY;
      case 'P': case 'Q': case 'R': case 'S':
        *out = ACX1_F1 + b - 'P';
        i = decode_modifiers_code(nl == 2 ? n[1] : 0);
        if (i < 0) return DI_BAD;
        *out |= i;
        return DI_KEY;
      case 'a': *out = ACX1_CTRL | ACX1_UP;   return DI_KEY; // rxvt
      case 'b': *out = ACX1_CTRL | ACX1_DOWN; return DI_KEY; // rxvt
      case 'c': *out = ACX1_CTRL | ACX1_RIGHT;return DI_KEY; // rxvt
      case 'd': *out = ACX1_CTRL | ACX1_LEFT; return DI_KEY; // rxvt
      }


      return DI_BAD;
    }

    // decode utf8
    if (b == 0x7F)
    {
      *out = ACX1_ALT | ACX1_BACKSPACE;
      *used_len_p = 2;
      return DI_KEY;
    }
    *out = ACX1_ALT | b;
    *used_len_p = 2;
    return DI_KEY;
  }

  *used_len_p = 1;
  switch (b)
  {
  case 0x00: *out = ACX1_CTRL | ACX1_SPACE; return DI_KEY;
  case 0x08: *out = ACX1_BACKSPACE; return DI_KEY;
  case 0x09: *out = ACX1_TAB; return DI_KEY;
  case 0x0D: *out = ACX1_ENTER; return DI_KEY;
  // case 0x1B: //*out = ACX1_ESC; return DI_KEY;
  //   *out = 'e';
  //   *used_len_p = 1;
  //   return DI_KEY;
  case 0x7F:
    *out = ACX1_BACKSPACE;
    return DI_KEY;
  default:
    *out = ACX1_CTRL | 0x40 | b;
    return DI_KEY;
  }
}

/* worker_main *************************************************************/
static void * worker_main (void * arg)
{
  int sr, n, di, mode, ofs;
  fd_set rfds;
  struct timeval tv;
  char cmd;
  uint8_t buf[0x100];
  char tmp[0x400];
  uint32_t dec[0x10];
  size_t ilen;

  LI("worker: enter\n");
  for (;;)
  {
    FD_ZERO(&rfds);
    FD_SET(worker_pipe[0], &rfds);
    FD_SET(tty_fd, &rfds);

    n = worker_pipe[0];
    if (n < tty_fd) n = tty_fd;

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    LI("worker: select()\n");
    sr = select(n + 1, &rfds, NULL, NULL, &tv);
    if (sr < 0)
    {
      if (errno == EINTR) continue;
      n = errno;
      LE("worker: select() failed: %d = %s\n", n, strerror(n));
      break;
    }
    if (!sr) cmd = 'z'; else cmd = 0;

    if (FD_ISSET(worker_pipe[0], &rfds))
    {
      LI("worker: reading from worker_pipe\n");
      n = read(worker_pipe[0], &cmd, 1);
      if (n < 0)
      {
        if (errno == EINTR) continue;
        n = errno;
        LI("read(worker_pipe) failed: %d = %s\n", n, strerror(n));
      }
      if (n == 0)
      {
        LI("worker: worker_pipe closed!\n");
        break;
      }
    }

    if (FD_ISSET(tty_fd, &rfds))
    {
      LI("worker: reading from tty\n");
      for (ofs = 0, mode = 0;
           (n = read(tty_fd, &buf[ofs], sizeof(buf) - ofs)) > 0;
           ofs = n - ofs)
      {
        if (mode == 1) continue; // consume all
        LI("read(tty):%u \"%s\"\n", n, escstr(tmp, sizeof(tmp), buf, n));

        pthread_mutex_lock(&mutex);
        for (ofs = 0; ofs < n; ofs += ilen)
        {
          LI("decoding input (%u bytes): "
             "%02X %02X %02X %02X %02X %02X %02X %02X\n",
             n - ofs,
             ofs + 0 < n ? buf[ofs + 0] : 0xFF,
             ofs + 1 < n ? buf[ofs + 1] : 0xFF,
             ofs + 2 < n ? buf[ofs + 2] : 0xFF,
             ofs + 3 < n ? buf[ofs + 3] : 0xFF,
             ofs + 4 < n ? buf[ofs + 4] : 0xFF,
             ofs + 5 < n ? buf[ofs + 5] : 0xFF,
             ofs + 6 < n ? buf[ofs + 6] : 0xFF,
             ofs + 7 < n ? buf[ofs + 7] : 0xFF
            );

          di = decode_input(&buf[ofs], n - ofs, dec, &ilen, decode_mode);
          if (di == DI_KEY)
          {
            LI("storing km=0x%X\n", dec[0]);
            qpush1(dec[0]);
            if (waiting_for_event && queue_len == 1)
              pthread_cond_signal(&event_cond);
            if (ilen == 0)
            {
              LE("BUG: ilen=0 at buf=\"%s\"\n", &buf[ofs]);
              mode = 1;
              break;
            }
            continue;
          }
          if (di == DI_ESC)
          {
            decode_mode = 0;
            switch (dec[0])
            {
            case REPORT_CURSOR_POSITION_REPLY:
              LI("got cursor pos: row %u, col %u\n", dec[1], dec[2]);
              real_row = dec[1];
              real_col = dec[2];
              if (waiting_for_cursor) pthread_cond_signal(&cursor_cond);
              if (!writing) { user_row = real_row; user_col = real_col; }
              break;
            default:
              LI("ignoring escape 0x%X \"%.*s\"\n",
                 dec[0], (int) ilen, &buf[ofs]);
            }
            continue;
          }
          if (di == DI_BAD)
          {
            LW("could not decode \"%s\" (ofs %u)\n",
               escstr(tmp, sizeof(tmp), &buf[ofs], n - ofs), ofs);
            mode = 1; // consume all
            ofs = n;
            break;
          }
          if (di == DI_MORE) break;
        }
        pthread_mutex_unlock(&mutex);

        if (ofs < n)
        {
          memmove(&buf[0], &buf[ofs], n - ofs);
        }

      }

      if (n < 0)
      {
        n = errno;
        if (n == EINTR)
        {
          LI("read(tty) interrupted by signal\n");
          continue;
        }
        if (n == EAGAIN)
        {
          LI("read(tty) finished input buffer\n");
          continue;
        }
        LW("worker: read(tty) failed: %d = %s\n", n, strerror(n));
        break;
      }
      if (n == 0) continue;
      /*
      {
        LI("worker: tty closed\n");
        break;
      }
      */
    }

    if (cmd == 'z')
    {
      struct winsize wsz;
      LI("reading terminal size...\n");
      if (ioctl(tty_fd, TIOCGWINSZ, &wsz))
      {
        n = errno;
        LW("failed to get terminal size with ioctl (error %d = %s)\n",
           n, strerror(n));
      }

      pthread_mutex_lock(&mutex);
      if (screen_height != wsz.ws_row || screen_width != wsz.ws_col)
      {
        screen_height = wsz.ws_row;
        screen_width = wsz.ws_col;
        screen_resized = 1;
        if (waiting_for_event) pthread_cond_signal(&event_cond);
      }
      pthread_mutex_unlock(&mutex);
    }
  }
  LI("exit worker\n");

  return arg;
}

/* acx1_init ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_init ()
{
  unsigned int rc;
  struct termios tio;
  struct winsize wsz;
  struct sigaction sa;

  /* init globals */
  tty_fd = -1;
  worker_pipe[0] = -1;
  worker_pipe[1] = -1;
  tio_set = 0;
  th_created = 0;
  mutex_created = 0;
  event_cond_created = 0;
  cursor_cond_created = 0;
  waiting_for_event = 0;
  waiting_for_cursor = 0;
  screen_resized = 0;
  queue_a = NULL;
  worker_error = 0;
  sigwinch_set = 0;
  writing = 0;

  Z(pipe(worker_pipe), ACX1_CREATE_PIPE_ERROR);

  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = winch_signal;
  Z(sigaction(SIGWINCH, &sa, NULL), ACX1_SIGNAL_ERROR);
  sigwinch_set = 1;

  Z(pthread_mutex_init(&mutex, NULL), ACX1_THREAD_ERROR);
  mutex_created = 1;

  Z(pthread_cond_init(&event_cond, NULL), ACX1_THREAD_ERROR);
  event_cond_created = 1;

  Z(pthread_cond_init(&cursor_cond, NULL), ACX1_THREAD_ERROR);
  cursor_cond_created = 1;

  Z(pthread_cond_init(&read_event_done_cond, NULL), ACX1_THREAD_ERROR);
  read_event_done_created = 1;

  tty_fd = open("/dev/tty", O_RDWR | O_NONBLOCK);
  C(tty_fd >= 0, ACX1_TERM_OPEN_FAILED);

  Z(tcgetattr(tty_fd, &orig_tio), ACX1_TERM_IO_FAILED);
  Z(ioctl(tty_fd, TIOCGWINSZ, &wsz), ACX1_TERM_IO_FAILED);
  screen_height = wsz.ws_row;
  screen_width = wsz.ws_col;
  Z(tcflush(tty_fd, TCIOFLUSH), ACX1_TERM_IO_FAILED);

  tio = orig_tio;
  tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                   INLCR | IGNCR | ICRNL | IXON);
  tio.c_oflag &= ~OPOST;
  tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tio.c_cflag &= ~(CSIZE | PARENB);
  tio.c_cflag |= (CS8);
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;

  Z(tcsetattr(tty_fd, TCSANOW, &tio), ACX1_TERM_IO_FAILED);
  tio_set = 1;

  // z(tty_write_const(S7C1T), ACX1_TERM_IO_FAILED);
  // Z(tty_write_const(SET_ANSI_CONFORMANCE_LEVEL_1), ACX1_TERM_IO_FAILED);
  Z(tty_write_const(APPLICATION_KEYPAD), ACX1_TERM_IO_FAILED);
  Z(tty_write_const(REPORT_CURSOR_POSITION), ACX1_TERM_IO_FAILED);
  decode_mode = 1;
  Z(tty_write_const(SHOW_CURSOR), ACX1_TERM_IO_FAILED);
  cursor_mode = 1;
  Z(tty_write_const(NO_WRAPAROUND_MODE), ACX1_TERM_IO_FAILED);
  Z(tty_write_const(BACKARROW_SENDS_DEL), ACX1_TERM_IO_FAILED);

  queue_shift = 6;
  queue_len = 0;
  qx_mask = (1 << queue_shift) - 1;
  qx_begin = 0;
  qx_end  = 0;
  queue_a = malloc(sizeof(uint32_t) << queue_shift);
  C(queue_a, ACX1_NO_MEM);

  Z(pthread_create(&worker_th, NULL, worker_main, NULL), ACX1_THREAD_ERROR);
  th_created = 1;

  return 0;

l_fail:
  acx1_finish();
  return rc;
}

/* acx1_finish **************************************************************/
ACX1_API void ACX1_CALL acx1_finish ()
{
  int i;

  finishing = 1;

  if (sigwinch_set)
  {
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGWINCH, &sa, NULL))
    {
      i = errno;
      LW("failed resetting SIGWINCH handler (error %d = %s)\n", i, strerror(i));
    }
    sigwinch_set = 0;
  }

  if (worker_pipe[1] >= 0)
  {
    if (close(worker_pipe[1]))
    {
      i = errno;
      LW("failed closing write-end of worker pipe (error %d = %s)\n",
         i, strerror(i));
    }
    worker_pipe[1] = -1;
  }

  if (th_created)
  {
    i = pthread_join(worker_th, NULL);
    if (i) LW("joining worker thread failed (error %d = %s)\n",
              i, strerror(i));
    th_created = 0;
  }

  if (worker_pipe[0] >= 0)
  {
    if (close(worker_pipe[0]))
    {
      i = errno;
      LW("failed closing read-end of worker pipe (error %d = %s)\n",
         i, strerror(i));
    }
    worker_pipe[0] = -1;
  }

  if (read_event_done_created)
  {
      LI("locking mutex\n");
      pthread_mutex_lock(&mutex);
      if (waiting_for_event)
      {
          LI("siglaling read_event\n");
          pthread_cond_signal(&event_cond);
          while (!read_event_done)
          {
              LI("waiting for read_event to be done\n");
              pthread_cond_wait(&read_event_done_cond, &mutex);
          }
          LI("read_event is done\n");
      }
      LI("unlocking mutex\n");
      pthread_mutex_unlock(&mutex);
      LI("unlocked mutex\n");
  }

  if (mutex_created)
  {
    i = pthread_mutex_destroy(&mutex);
    if (i) LW("failed destroying worker mutex (error %d = %s)\n",
              i, strerror(i));
  }

  if (event_cond_created)
  {
    i = pthread_cond_destroy(&event_cond);
    if (i) LW("failed destroying event condition variable (error %d = %s)\n",
              i, strerror(i));
  }

  if (cursor_cond_created)
  {
    i = pthread_cond_destroy(&cursor_cond);
    if (i) LW("failed destroying cursor condition variable (error %d = %s)\n",
              i, strerror(i));
  }

  if (queue_a) { free(queue_a); queue_a = NULL; }

  if (tio_set)
  {
    tio_set = 0;
    if (tcsetattr(tty_fd, TCSANOW, &orig_tio))
    {
      i = errno;
      LW("failed restoring termcap I/O state (error %d = %s)\n",
         i, strerror(i));
    }
  }

  if (tty_fd >= 0)
  {
    tty_write_const(WRAPAROUND_MODE);
    tty_write_const(NORMAL_KEYPAD);
    tty_write_const(SHOW_CURSOR);
    if (close(tty_fd))
    {
      i = errno;
      LW("failed closing terminal (error %d = %s)\n",
                          i, strerror(i));
    }
    tty_fd = -1;
  }
  LI("acx1 finished!\n");
}

/* acx1_get_screen_size *****************************************************/
ACX1_API unsigned int ACX1_CALL acx1_get_screen_size (uint16_t * h, uint16_t * w)
{
  pthread_mutex_lock(&mutex);
  *h = screen_height;
  *w = screen_width;
  pthread_mutex_unlock(&mutex);
  return ACX1_OK;
}

/* acx1_read_event **********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_read_event (acx1_event_t * event_p)
{
  unsigned int rc = 0;

  event_p->type = ACX1_NONE;
  pthread_mutex_lock(&mutex);

  for (;;)
  {
    LI("read_event: checking event\n");

    if (finishing)
    {
      LI("read_event: finishing\n");
      event_p->type = ACX1_FINISH;
      read_event_done = 1;
      pthread_cond_signal(&read_event_done_cond);
      break;
    }

    if (screen_resized)
    {
      LI("read_event: screen_resized\n");
      screen_resized = 0;
      event_p->type = ACX1_RESIZE;
      event_p->size.w = screen_width;
      event_p->size.h = screen_height;
      break;
    }

    if (queue_len)
    {
      LI("read_event: key_press; queue_len=%d\n", queue_len);
      event_p->type = ACX1_KEY;
      event_p->km = qpop1();
      break;
    }

    LI("read_event: waiting for event\n");
    waiting_for_event = 1;
    pthread_cond_wait(&event_cond, &mutex);
    waiting_for_event = 0;
  }
//l_fail:
  pthread_mutex_unlock(&mutex);

  return rc;
}

/* acx1_get_cursor_pos ******************************************************/
ACX1_API unsigned int ACX1_CALL acx1_get_cursor_pos (uint16_t * r, uint16_t * c)
{
  pthread_mutex_lock(&mutex);
  while (!real_row)
  {
    waiting_for_cursor = 1;
    pthread_cond_wait(&cursor_cond, &mutex);
    waiting_for_cursor = 0;
  }
  *r = user_row;
  *c = user_col;
  pthread_mutex_unlock(&mutex);
  return 0;
}

/* acx1_set_cursor_pos ******************************************************/
ACX1_API unsigned int ACX1_CALL acx1_set_cursor_pos (uint16_t r, uint16_t c)
{
  uint8_t buf[0x40];
  int rc;
  uint_t cpos_len;

  pthread_mutex_lock(&mutex);
  if (writing)
  {
    user_row = r;
    user_col = c;
    rc = 0;
  }
  else rc = -1;
  pthread_mutex_unlock(&mutex);
  if (rc >= 0) return rc;

  cpos_len = set_cursor_pos_str(buf, r, c);
  sprintf((char *) buf, "\e[%u;%uH", r, c);
  if (tty_write(buf, cpos_len)) return ACX1_TERM_IO_FAILED;

  pthread_mutex_lock(&mutex);
  user_row = real_row = r;
  user_col = real_col = c;
  pthread_mutex_unlock(&mutex);
  return 0;
}

/* acx1_write_pos ***********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write_pos (uint16_t r, uint16_t c)
{
  char buf[0x40];
  int rc;

  pthread_mutex_lock(&mutex);
  if (!writing) rc = ACX1_NOT_WRITING;
  else
  {
    rc = -1;
    real_row = r;
    real_col = c;
  }
  pthread_mutex_unlock(&mutex);
  if (rc >= 0) return rc;

  sprintf(buf, "\e[%u;%uH", r, c);
  if (tty_write(buf, strlen(buf))) return ACX1_TERM_IO_FAILED;

  return 0;
}

/* acx1_clear ***************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_clear ()
{
  unsigned int rc;
  uint16_t r;
  pthread_mutex_lock(&mutex);
  rc = !writing ? ACX1_NOT_WRITING : 0;
  pthread_mutex_unlock(&mutex);
  if (rc) return rc;
  for (r = 1; r <= screen_height; ++r)
  {
    rc = acx1_write_pos(r, 1);
    if (rc) return rc;
    rc = acx1_fill(' ', screen_width);
    if (rc) return rc;
  }

  // Z(tty_write_const(CLEAR_ALL_SCREEN), ACX1_TERM_IO_FAILED); 
  return 0;

// l_fail:
//   return rc;
}

/* acx1_get_cursor_mode *****************************************************/
ACX1_API unsigned int ACX1_CALL acx1_get_cursor_mode (uint8_t * mode_p)
{
  pthread_mutex_lock(&mutex);
  *mode_p = cursor_mode;
  pthread_mutex_unlock(&mutex);
  return 0;
}

/* acx1_set_cursor_mode *****************************************************/
ACX1_API unsigned int ACX1_CALL acx1_set_cursor_mode (uint8_t mode)
{
  unsigned int rc;
  pthread_mutex_lock(&mutex);
  if (cursor_mode == mode) mode = 2;
  else cursor_mode = mode;
  if (writing) mode = 2;
  pthread_mutex_unlock(&mutex);
  if ((mode == 0 && tty_write_const(HIDE_CURSOR)) ||
      (mode == 1 && tty_write_const(SHOW_CURSOR))) rc = ACX1_TERM_IO_FAILED;
  else rc = ACX1_OK;
  return rc;
}

/* acx1_write_start *********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write_start ()
{
  unsigned int rc;

  pthread_mutex_lock(&mutex);
  if (writing) rc = ACX1_ALREADY_WRITING;
  else
  {
    writing = 1;
    rc = ACX1_OK;
  }
  pthread_mutex_unlock(&mutex);
  if (rc) goto l_fail;
  Z(tty_write_const(HIDE_CURSOR), ACX1_TERM_IO_FAILED);
l_fail:
  return rc;
}

/* acx1_write_stop **********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write_stop ()
{
  unsigned int rc, r, c;
  int cm;

  pthread_mutex_lock(&mutex);
  writing = 0;
  cm = cursor_mode;
  r = user_row;
  c = user_col;
  pthread_mutex_unlock(&mutex);
  if (cm && tty_write_const(SHOW_CURSOR)) rc = ACX1_TERM_IO_FAILED;
  rc = acx1_set_cursor_pos(r, c);
  return rc;
}

/* acx1_write ***************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write (void const * data, size_t len)
{
  return tty_write(data, len) ? ACX1_TERM_IO_FAILED : ACX1_OK;
}

/* acx1_fill ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_fill (uint32_t ch, uint16_t count)
{
  char buf[0x40];
  unsigned int bl;

  if (ch >= 0x7F) return ACX1_NO_CODE;
  bl = sizeof(buf);
  if (bl > count) bl = count;
  memset(buf, ch, bl);
  while (count)
  {
    if (count < bl) bl = count;
    if (tty_write(buf, bl)) break;
    count -= bl;
  }

  return count ? ACX1_TERM_IO_FAILED : ACX1_OK;
}

/* acx1_attr ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_attr
(
  int bg,
  int fg,
  unsigned int mode
)
{
  uint8_t buf[0x80];
  uint_t len;
  len = set_attr_str(buf, bg, fg, mode);
  return tty_write(buf, len) ? ACX1_TERM_IO_FAILED : ACX1_OK;
}

/* acx1_rect ****************************************************************/
ACX1_API uint_t ACX1_CALL acx1_rect
(
  uint8_t const * const * data, // array of rows of utf8 text with special escapes
  uint16_t start_row,
  uint16_t start_col,
  uint16_t row_num, 
  uint16_t col_num,
  acx1_attr_t * attrs
)
{
  static uint8_t buf[0x10000];
#define BLIM (sizeof(buf) - 0x80)
  int rc;
  int chunk_attr = 0, crt_attr = -1;
  uint_t i, row_ofs; // byte offset in current row
  int row_width_left = 0; // width left in current row
  size_t chunk_len, chunk_cps, chunk_width;
  size_t buf_len;
  char new_line;

  /* easy peasy? */
  if (start_row > screen_height || start_col > screen_width) return 0;

  if (row_num > screen_height - start_row + 1) row_num = screen_height - start_row + 1;
  if (col_num > screen_width - start_col + 1) col_num = screen_width - start_col + 1;

  for (new_line = 1, row_ofs = 0, i = 0, buf_len = 0; i < row_num; )
  {
    //LI("nl=%u,row_ofs=0x%X,row=0x%X,buf_len=0x%X,width_left=0x%X\n", new_line, row_ofs, i, (int) buf_len, row_width_left);
    if (buf_len >= BLIM) goto l_write;

    if (new_line)
    {
      buf_len += set_cursor_pos_str(&buf[buf_len], start_row + i, start_col);
      new_line = 0;
      row_ofs = 0;
      row_width_left = col_num;
      chunk_attr = 0;
    }

    if (data[i][row_ofs] == 0)
    {
      //LI("end of row. buf_len=0x%X, width_left=0x%X\n", (int) buf_len, row_width_left);
      // end of row
      if (row_width_left)
      {
        if (buf_len + row_width_left >= BLIM) chunk_len = BLIM - buf_len;
        else chunk_len = row_width_left;
        //LI("clearing to EOL. len=0x%X\n", (int) chunk_len);
        memset(&buf[buf_len], ' ', chunk_len);
        row_width_left -= chunk_len;
        buf_len += chunk_len;
        if (row_width_left) goto l_write;
      }

      new_line = 1;
      ++i;
      continue;
    }
    else if (data[i][row_ofs] == '\a')
    {
      // attr change
      chunk_attr = data[i][row_ofs + 1];
      row_ofs += 2;
      continue;
    }

    //LI("measure(blim=0x%X,wlim=0x%X,str:%s)\n", (int) (BLIM - buf_len), row_width_left, data[i] + row_ofs);
    rc = acx1_utf8_str_measure(acx1_term_char_width_wctx, NULL, data[i] + row_ofs,
                              BLIM - buf_len, SSIZE_MAX, row_width_left, 
                              &chunk_len, &chunk_cps, &chunk_width);
    //LI(".. rc=%d: len=0x%X, cps=0x%X, width=0x%X\n", rc, (int) chunk_len, (int) chunk_cps, (int) chunk_width);
    if (rc < 0 && !chunk_len)
    {
      // malformed utf8; this could happen because of passing a truncated line
      if (buf_len >= BLIM - 4) goto l_write; // maybe we truncated the input
      // there's enough room to output one codepoint so the string is malformed
      // or there's a non-printable other than NUL and \a
      //LI("passed bad data\n");
      return ACX1_BAD_DATA;
    }

    if (chunk_attr != crt_attr)
    {
      crt_attr = chunk_attr;
      buf_len += set_attr_str(&buf[buf_len], 
                              attrs[crt_attr].bg, attrs[crt_attr].fg,
                              attrs[crt_attr].mode);
    }
    memcpy(&buf[buf_len], data[i] + row_ofs, chunk_len);
    buf_len += chunk_len;
    row_ofs += chunk_len;
    row_width_left -= chunk_width;
    if (!row_width_left) { new_line = 1; ++i; continue; }
    if (buf_len < BLIM - 4) continue;

l_write:
    //LI("writing %lu bytes\n", buf_len);
    if (tty_write(buf, buf_len)) return ACX1_TERM_IO_FAILED;
    buf_len = 0;
  }

  //LI("writing %lu bytes\n", buf_len);
  if (buf_len && tty_write(buf, buf_len)) return ACX1_TERM_OPEN_FAILED;

  return 0;
}

#endif
