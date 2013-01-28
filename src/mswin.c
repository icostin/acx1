/* acx1 - Application Console Interface - ver. 1
 *
 * Windows console support
 *
 * Changelog:
 *  - 2013/01/06 Costin Ionescu: initial release
 *  - 2013/01/17 Costin Ionescu: writing with WriteConsoleW so that wide-chars
 *    are printed ok
 */
#include <windows.h>
#include <c41.h>
#include <acx1.h>

static FILE * log_file = NULL;
static int log_level = 0;
static HANDLE hin, hout;
static uint16_t screen_height, screen_width;
static uint16_t cursor_row, cursor_col;
static uint16_t write_row, write_col;
static uint16_t attr;
static DWORD orig_in_mode, orig_out_mode;
static CONSOLE_CURSOR_INFO cci;

/* acx1_name ****************************************************************/
ACX1_API char const * ACX1_CALL acx1_name ()
{
  return "acx1-ms_windows";
}

/* acx1_set_log_file ********************************************************/
ACX1_API void acx1_logging (int level, FILE * lf)
{
  log_file = lf;
  log_level = level;
}

/* acx1_init ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_init ()
{
  unsigned int rc;
  CONSOLE_SCREEN_BUFFER_INFO csbi;

  hout = hin = INVALID_HANDLE_VALUE;

  hin = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                    NULL, OPEN_EXISTING, 0, NULL);
  if (hin == INVALID_HANDLE_VALUE) return ACX1_TERM_OPEN_FAILED;

  hout = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, 0, NULL);
  if (hout == INVALID_HANDLE_VALUE) { rc = ACX1_TERM_OPEN_FAILED; goto l_fail; }

  if (!GetConsoleScreenBufferInfo(hout, &csbi)) 
  { rc = ACX1_TERM_IO_FAILED; goto l_fail; }

  if (!GetConsoleCursorInfo(hout, &cci))
  { rc = ACX1_TERM_IO_FAILED; goto l_fail; }

  if (!GetConsoleMode(hin, &orig_in_mode))
  { rc = ACX1_TERM_IO_FAILED; goto l_fail; }
  if (!GetConsoleMode(hout, &orig_out_mode))
  { rc = ACX1_TERM_IO_FAILED; goto l_fail; }

  if (!SetConsoleMode(hin, ENABLE_QUICK_EDIT_MODE))
  { rc = ACX1_TERM_IO_FAILED; goto l_fail; }
  if (!SetConsoleMode(hout, 0))
  { rc = ACX1_TERM_IO_FAILED; goto l_fail; }

  screen_width = csbi.dwSize.X;
  screen_height = csbi.dwSize.Y;
  cursor_col = csbi.dwCursorPosition.X;
  cursor_row = csbi.dwCursorPosition.Y;
  return 0;
l_fail:
  acx1_finish();
  return rc;
}

/* acx1_finish **************************************************************/
ACX1_API void ACX1_CALL acx1_finish ()
{
  if (hin != INVALID_HANDLE_VALUE) CloseHandle(hin);
  if (hout != INVALID_HANDLE_VALUE) CloseHandle(hout);
}

/* acx1_read_event **********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_read_event (acx1_event_t * event_p)
{
  INPUT_RECORD ir;
  DWORD n;
  uint16_t ch;
  uint32_t m;

  for (;;)
  {
    if (!ReadConsoleInputW(hin, &ir, 1, &n)) return ACX1_TERM_IO_FAILED;
    if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT)
    {
      event_p->type = ACX1_RESIZE;
      event_p->size.w = ir.Event.WindowBufferSizeEvent.dwSize.X;
      event_p->size.h = ir.Event.WindowBufferSizeEvent.dwSize.Y;
      return 0;
    }
    if (ir.EventType == KEY_EVENT)
    {
      if (!ir.Event.KeyEvent.bKeyDown) continue;
      m = 0;
      if (ir.Event.KeyEvent.dwControlKeyState & 
          (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
        m |= ACX1_ALT;
      if (ir.Event.KeyEvent.dwControlKeyState & 
          (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
        m |= ACX1_CTRL;
      if (ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)
        m |= ACX1_SHIFT;
      event_p->type = ACX1_KEY;
      ch = ir.Event.KeyEvent.uChar.UnicodeChar;
      if (ch >= 0x20) 
        event_p->km = (m & ~ACX1_SHIFT) | ir.Event.KeyEvent.uChar.UnicodeChar;
      else //if (ch > 0)
      {

        if (ch == 0x08 || ch == 0x09 || ch == 0x0D) m = ch;
        else 
        {
          // event_p->km = ACX1_CTRL | 0x40 | ch;
          switch (ch = ir.Event.KeyEvent.wVirtualKeyCode)
          {
          case VK_BACK:         m |= ACX1_BACKSPACE;            break;
          case VK_TAB:          m |= ACX1_TAB;                  break;
          case VK_RETURN:       m |= ACX1_ENTER;                break;
          case VK_ESCAPE:       m |= ACX1_ESC;                  break;
          case VK_SPACE:        m |= ACX1_SPACE;                break;
          case VK_PRIOR:        m |= ACX1_PAGE_UP;              break;
          case VK_NEXT:         m |= ACX1_PAGE_DOWN;            break;
          case VK_END:          m |= ACX1_END;                  break;
          case VK_HOME:         m |= ACX1_HOME;                 break;
          case VK_LEFT:         m |= ACX1_LEFT;                 break;
          case VK_UP:           m |= ACX1_UP;                   break;
          case VK_RIGHT:        m |= ACX1_RIGHT;                break;
          case VK_DOWN:         m |= ACX1_DOWN;                 break;
          case VK_INSERT:       m |= ACX1_INS;                  break;
          case VK_DELETE:       m |= ACX1_DEL;                  break;
          case VK_F1:           m |= ACX1_F1;                   break;
          case VK_F2:           m |= ACX1_F2;                   break;
          case VK_F3:           m |= ACX1_F3;                   break;
          case VK_F4:           m |= ACX1_F4;                   break;
          case VK_F5:           m |= ACX1_F5;                   break;
          case VK_F6:           m |= ACX1_F6;                   break;
          case VK_F7:           m |= ACX1_F7;                   break;
          case VK_F8:           m |= ACX1_F8;                   break;
          case VK_F9:           m |= ACX1_F9;                   break;
          case VK_F10:          m |= ACX1_F10;                  break;
          case VK_F11:          m |= ACX1_F11;                  break;
          case VK_F12:          m |= ACX1_F12;                  break;
          default:
            if ((ch >= '0' && ch <= '9') || 
                (ch >= 'A' && ch <= 'Z')) m |= ch;
            else continue;
          }
        }

        event_p->km = m;
      }
      // else { event_p->km = '?'; }

      return 0;
    }
  }
}

/* acx1_set_cursor_mode *****************************************************/
ACX1_API unsigned int ACX1_CALL acx1_set_cursor_mode (uint8_t mode)
{
  cci.bVisible = (mode != 0);
  if (!SetConsoleCursorInfo(hout, &cci))  return ACX1_TERM_IO_FAILED;
  return 0;
}

/* acx1_get_cursor_mode *****************************************************/
ACX1_API unsigned int ACX1_CALL acx1_get_cursor_mode (uint8_t * mode_p)
{
  return cci.bVisible;
}

/* acx1_set_cursor_pos ******************************************************/
ACX1_API unsigned int ACX1_CALL acx1_set_cursor_pos (uint16_t r, uint16_t c)
{
  COORD p;
  p.Y = cursor_row = r - 1;
  p.X = cursor_col = c - 1;
  if (!SetConsoleCursorPosition(hout, p)) return ACX1_TERM_IO_FAILED;
  return 0;
}

/* acx1_get_cursor_pos ******************************************************/
ACX1_API unsigned int ACX1_CALL acx1_get_cursor_pos (uint16_t * r, uint16_t * c)
{
  *r = cursor_row + 1;
  *c = cursor_col + 1;
  return 0;
}

/* acx1_get_screen_size *****************************************************/
ACX1_API unsigned int ACX1_CALL acx1_get_screen_size 
(
  uint16_t * h, 
  uint16_t * w
)
{
  // TODO: lock mutex
  *h = screen_height;
  *w = screen_width;
  return 0;
}

/* acx1_write_start *********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write_start ()
{
  write_row = cursor_row;
  write_col = cursor_col;
  if (cci.bVisible)
  {
    char b;
    cci.bVisible = 0;
    b = SetConsoleCursorInfo(hout, &cci);
    cci.bVisible = 1;
    if (!b) return ACX1_TERM_IO_FAILED;
  }

  return 0;
}

/* acx1_charset *************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_charset (unsigned int cs)
{
  return ACX1_NO_CODE;
}

/* prepare_attr *************************************************************/
static uint16_t ACX1_CALL prepare_attr (int bg, int fg, int mode)
{
  uint16_t a;
  (void) mode;
  a = 0;
  if ((bg & 1)) a |= BACKGROUND_RED;
  if ((bg & 2)) a |= BACKGROUND_GREEN;
  if ((bg & 4)) a |= BACKGROUND_BLUE;
  if ((bg & 8)) a |= BACKGROUND_INTENSITY;
  if ((fg & 1)) a |= FOREGROUND_RED;
  if ((fg & 2)) a |= FOREGROUND_GREEN;
  if ((fg & 4)) a |= FOREGROUND_BLUE;
  if ((fg & 8)) a |= FOREGROUND_INTENSITY;
  return a;
}

/* acx1_attr ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_attr (int bg, int fg, unsigned int mode)
{
  attr = prepare_attr(bg, fg, mode);
  //if (!SetConsoleTextAttribute(hout, attr)) return ACX1_TERM_IO_FAILED;

  return 0;
}

/* acx1_write_pos ***********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write_pos (uint16_t r, uint16_t c)
{
  COORD p;
  p.Y = write_row = r - 1;
  p.X = write_col = c - 1;
  if (!SetConsoleCursorPosition(hout, p)) return ACX1_TERM_IO_FAILED;
  return 0;
}

/* acx1_write ***************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write (void const * data, size_t len)
{
//  WORD buf[0x200];
//  DWORD wc;
//  size_t cpc, width, bl, cl;
//  int c;
//
//  c = c41_utf8_str_measure(c41_term_char_width_wctx, NULL,
//                           data, len, C41_SSIZE_MAX, screen_width - write_col,
//                           &bl, &cl, &width);
//  if (c < 0) return ACX1_BAD_DATA;
//
//  c = c41_mutf8_str_decode(data, len, buf, C41_ITEM_COUNT(buf), NULL, &cpc);
//  if (c) return ACX1_BAD_DATA;
//
//  if (!WriteConsoleW(hout, buf, cpc, &wc, NULL)) return ACX1_TERM_IO_FAILED;
//
//  write_col += width;
//  if (write_col > screen_width) write_col = 0;
//
//  return 0;

  uint16_t buf[0x200];
  size_t cpc;
  int c;
  CHAR_INFO ci[0x200];
  size_t i, j, l;
  SMALL_RECT wr;
  COORD bs, bc;
  //uint8_t * b;

  c = c41_mutf8_str_decode(data, len, buf, C41_ITEM_COUNT(buf), NULL, &cpc);
  if (c) return ACX1_BAD_DATA;

  //b = data; cpc = len;
  l = C41_ITEM_COUNT(ci);
  for (c = 0, i = j = 0; c >= 0 && i < cpc && j < l; ++i, j += c)
  {
    ci[j].Char.UnicodeChar = buf[i];
    ci[j].Attributes = attr;
    c = c41_term_char_width(buf[i]);
  }
  if (c < 0) return ACX1_BAD_DATA;
  if (!j) return 0; // nothing to print

  bs.X = j;
  bs.Y = 1;
  bc.X = 0;
  bc.Y = 0;
  wr.Left = write_col;
  wr.Top = write_row;
  wr.Right = screen_width - 1;
  wr.Bottom = write_row;
  if (!WriteConsoleOutputW(hout, ci, bs, bc, &wr))
    return ACX1_TERM_IO_FAILED;
  write_col = wr.Right + 1;

  return 0;
}

/* acx1_fill ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_fill (uint32_t ch, uint16_t count)
{
  COORD co;
  DWORD w;
  if (ch >= 0x10000) return ACX1_NOT_SUPPORTED;
  co.X = write_col;
  co.Y = write_row;
  if (!FillConsoleOutputCharacter(hout, ch, count, co, &w))
    return ACX1_TERM_IO_FAILED;
  if (!FillConsoleOutputAttribute(hout, attr, count, co, &w))
    return ACX1_TERM_IO_FAILED;
  write_col += count;
  if (write_col >= screen_width) write_col = screen_width - 1;
  // co.Y = write_row;
  // co.X = write_col;
  // if (!SetConsoleCursorPosition(hout, co)) return ACX1_TERM_IO_FAILED;
  return 0;
}

/* acx1_clear ***************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_clear ()
{
  write_row = write_col = 0;
  return acx1_fill(' ', screen_height * screen_width);
}

/* acx1_write_stop **********************************************************/
ACX1_API unsigned int ACX1_CALL acx1_write_stop ()
{
  COORD p;
  if (cci.bVisible &&
      !SetConsoleCursorInfo(hout, &cci)) return ACX1_TERM_IO_FAILED;
  p.Y = cursor_row;
  p.X = cursor_col;
  if (!SetConsoleCursorPosition(hout, p)) return ACX1_TERM_IO_FAILED;
  return 0;
}

/* acx1_rect ****************************************************************/
ACX1_API unsigned int ACX1_CALL acx1_rect
(
  uint8_t const * const * data, // array of rows of utf8 text with special escapes
  uint16_t start_row,
  uint16_t start_col,
  uint16_t row_num, 
  uint16_t col_num,
  acx1_attr_t * attrs
)
{
  CHAR_INFO buf[0x2000];
  CHAR_INFO * ci;
  SMALL_RECT wr;
  COORD bs, bc;
  uint_t i, j, o, crt_attr, buf_rows;
  int l, cw;
  uint16_t a;
  uint32_t cp;


  start_row--; start_col--;
  if (start_row >= screen_height || start_col >= screen_width) return 0;
  if (row_num > screen_height - start_row) row_num = screen_height - start_row;
  if (col_num > screen_width - start_col) col_num = screen_width - start_col;

  buf_rows = C41_ITEM_COUNT(buf) / col_num;
  bs.X = col_num;
  bc.X = 0;
  bc.Y = 0;
  for (; row_num; row_num -= buf_rows, start_row += buf_rows, data += buf_rows)
  {
    if (row_num < buf_rows) buf_rows = row_num;
    for (ci = &buf[0], i = 0; i < buf_rows; ++i)
    {
      crt_attr = 0;
      a = prepare_attr(attrs[crt_attr].bg, attrs[crt_attr].fg, 
                       attrs[crt_attr].mode);

      for (j = o = 0; data[i][o] && j < col_num; o += l, j += cw, ci += cw)
      {
        if (data[i][o] >= 0x80)
        {
          l = c41_utf8_char_decode_strict(data[i] + o, C41_SSIZE_MAX, &cp);
          if (l < 0) return ACX1_BAD_DATA;
          cw = c41_term_char_width(cp);
          if (cw < 0) return ACX1_BAD_DATA;
          if (!cw) continue;
          if (cw == 2)
          {
            if (j == col_num - 1) break; // finish line if a wide char is in last column
            ci[1].Attributes = a;
            ci[1].Char.UnicodeChar = 0;
          }
        }
        else 
        { 
          cp = data[i][o];
          if (cp == '\a')
          {
            crt_attr = data[i][o + 1];
            a = prepare_attr(attrs[crt_attr].bg, attrs[crt_attr].fg, 
                             attrs[crt_attr].mode);
            cw = 0;
            l = 2;
            continue;
          }
          if (cp < 0x20) ACX1_BAD_DATA;
          l = 1; 
          cw = 1;
        }
        ci->Char.UnicodeChar = (uint16_t) (cp >= 0x10000 ? 0 : cp);
        ci->Attributes = a;
      }
      if (j < col_num)
      {
        crt_attr = 0;
        a = prepare_attr(attrs[crt_attr].bg, attrs[crt_attr].fg, 
                         attrs[crt_attr].mode);
        for (; j < col_num; ++j, ++ci) 
        { ci->Char.UnicodeChar = ' '; ci->Attributes = a; }
      }

    }
    bs.Y = buf_rows;
    wr.Left = start_col;
    wr.Top = start_row;
    wr.Right = start_col + col_num - 1;
    wr.Bottom = start_row + buf_rows - 1;
    if (!WriteConsoleOutputW(hout, buf, bs, bc, &wr)) 
      return ACX1_TERM_IO_FAILED;
  }

  return 0;
}

