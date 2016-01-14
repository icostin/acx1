/* acx1 - Application Console Interface - ver. 1
 *
 * Common code for all platforms.
 *
 * Changelog:
 *  - 2013/01/06 Costin Ionescu: initial release
 *
 */
#include <string.h>
#include <stdio.h>
#include "acx1.h"

extern unsigned char acx1_ucw_ofs_a[];
extern unsigned char acx1_ucw_val_a[];

static char acx1_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* acx1_hexz *****************************************************************/
ACX1_API void * ACX1_CALL acx1_hexz (void * out, void const * in, size_t len)
{
  uint8_t const * i = in;
  uint8_t const * ie = i + len;
  uint8_t * o = out;

  for (; i < ie; ++i)
  {
    *o++ = acx1_digits[*i >> 4];
    *o++ = acx1_digits[*i & 0x0F];
  }
  *o = 0;
  return out;
}

/* acx1_utf8_char_len ********************************************************/
ACX1_API int ACX1_CALL acx1_utf8_char_len (uint32_t cp)
{
  if (cp < 0x80) return 1;
  if (cp < 0x800) return 2;
  if (cp < 0x10000) return ((cp & 0xF800) == 0xD800) ? -3 : 3;
  if (cp < 0x110000) return 4;
  return -1;
}

/* acx1_utf8_char_decode_raw *************************************************/
ACX1_API int ACX1_CALL acx1_utf8_char_decode_raw
(
  void const * vdata,
  size_t len,
  uint32_t * out
)
{
  uint8_t const * data = vdata;
  uint8_t b;
#define CC(_pos) if ((data[_pos] & 0xC0) != 0x80) return -2 - (_pos);
  if (!len) return -1;
  b = *data;
  if (b < 0x80) { *out = b; return 1; }
  if (b < 0xC0) return -2;
  if (b < 0xE0)
  {
    if (len < 2) return -1;
    CC(1);
    *out = (((uint32_t) b & 0x1F) << 6) | (data[1] & 0x3F);
    return 2;
  }
  if (b < 0xF0)
  {
    if (len < 3) return -1;
    CC(1);
    CC(2);
    *out =  (((uint32_t) b & 0x0F) << 12) |
            (((uint32_t) data[1] & 0x3F) << 6) |
            ((uint32_t) data[2] & 0x3F);
    return 3;
  }
  if (b < 0xF8)
  {
    if (len < 4) return -1;
    CC(1);
    CC(2);
    CC(3);
    *out =  (((uint32_t) b & 0x07) << 18) |
            (((uint32_t) data[1] & 0x3F) << 12) |
            (((uint32_t) data[2] & 0x3F) << 6) |
            ((uint32_t) data[3] & 0x3F);
    return 4;
  }
  if (b < 0xFC)
  {
    if (len < 5) return -1;
    CC(1);
    CC(2);
    CC(3);
    CC(4);
    *out =  (((uint32_t) b & 0x03) << 24) |
            (((uint32_t) data[1] & 0x3F) << 18) |
            (((uint32_t) data[2] & 0x3F) << 12) |
            (((uint32_t) data[3] & 0x3F) << 6) |
            ((uint32_t) data[4] & 0x3F);
    return 5;
  }
  if (b < 0xFE)
  {
    if (len < 6) return -1;
    CC(1);
    CC(2);
    CC(3);
    CC(4);
    CC(5);
    *out =  (((uint32_t) b & 0x01) << 30) |
            (((uint32_t) data[1] & 0x3F) << 24) |
            (((uint32_t) data[2] & 0x3F) << 18) |
            (((uint32_t) data[3] & 0x3F) << 12) |
            (((uint32_t) data[4] & 0x3F) << 6) |
            ((uint32_t) data[5] & 0x3F);
    return 6;
  }
  return -2;
#undef CC
}

/* acx1_utf8_char_decode_strict **********************************************/
ACX1_API int ACX1_CALL acx1_utf8_char_decode_strict
(
  void const * vdata,
  size_t len,
  uint32_t * out
)
{
  int l;
  l = acx1_utf8_char_decode_raw(vdata, len, out);
  if (l < 0) return l;
  if (acx1_utf8_char_len(*out) != l) return -8;
  if (l >= 0x110000) return -9;
  if ((l & 0xFFF800) == 0x00D800) return -10;
  return l;
}

/* acx1_utf8_str_measure *****************************************************/
ACX1_API int ACX1_CALL acx1_utf8_str_measure
(
  int (ACX1_CALL * wf) (uint32_t cp, void * ctx),
  void * wf_ctx,
  void const * data,
  size_t b,
  size_t c,
  size_t w,
  size_t * bp,
  size_t * cp,
  size_t * wp
)
{
  int rc, l, k;
  size_t lb, lc, lw, cb, cc, cw;
  uint32_t ch;
  uint8_t const * d;

  lb = lc = lw = cb = cc = cw = 0;
  d = data;
  for (; cb < b; lb = cb, lc = cc, lw = cw)
  {
    l = acx1_utf8_char_decode_strict(d, b - cb, &ch);
    if (l <= 0) { rc = -1; goto l_ret; }
    cb += l;
    d += l;
    ++cc;
    if (cc > c) { rc = 1; goto l_ret; }
    k = wf(ch, wf_ctx);
    if (k < 0) { rc = -2; goto l_ret; }
    cw += k;
    if (cw > w) { rc = 2; goto l_ret; }
  }
  rc = 0;

l_ret:
  *bp = lb;
  *cp = lc;
  *wp = lw;
  return rc;
}

/* acx1_term_char_width ******************************************************/
ACX1_API int ACX1_CALL acx1_term_char_width (uint32_t cp)
{
  int b;

  b = (acx1_ucw_val_a[(((uint_t) acx1_ucw_ofs_a[cp >> 8]) << 6)
                    + ((cp >> 2) & 0x3F)] >> (2 * (cp & 3))) & 3;
  return b == 3 ? -1 : b;
}

/* acx1_term_char_width_wctx *************************************************/
ACX1_API int ACX1_CALL acx1_term_char_width_wctx (uint32_t cp, void * ctx)
{
  (void) ctx;
  return acx1_term_char_width(cp);
}

ACX1_API int ACX1_CALL acx1_mutf8_str_decode
(
  void const * vdata,
  size_t len,
  uint16_t * out_a,
  size_t out_n,
  size_t * in_len_p,
  size_t * out_len_p
)
{
  uint8_t const * d = vdata;
  uint8_t const * e = d + len;
  uint32_t cp;
  int l;
  size_t olen;

  for (olen = 0; d < e && olen < out_n; d += l, ++olen)
  {
    l = acx1_utf8_char_decode_raw(d, e - d, &cp);
    if (l < 0) goto l_exitio;
    if (cp >= 0x10000) { l = -9; goto l_exitio; }
    if (!cp)
    {
      if (l != 2) { l = (l == 1) ? -10 : -8; goto l_exitio; }
    }
    else if (acx1_utf8_char_len(cp) != l) { l = -8; goto l_exitio; }
    out_a[olen] = (uint16_t) cp;
  }

  if (in_len_p) *in_len_p = d - (uint8_t const *) vdata;
  if (d < e)
  {
    // output full, just count from now on...
    for (; d < e; d += l, ++olen)
    {
      l = acx1_utf8_char_decode_raw(d, e - d, &cp);
      if (l < 0) goto l_exitio;
      if (cp >= 0x10000) { l = -9; goto l_exitio; }
      if (!cp)
      {
        if (l != 2) { l = (l == 1) ? -10 : -8; goto l_exitio; }
      }
      else if (acx1_utf8_char_len(cp) != l) { l = -8; goto l_exitio; }
    }
  }

  l = 0;

l_exito:
  if (out_len_p) *out_len_p = olen;
  return l;

l_exitio:
  if (in_len_p) *in_len_p = d - (uint8_t const *) vdata;
  goto l_exito;
}






/* acx1_status_str **********************************************************/
ACX1_API char const * ACX1_CALL acx1_status_str (unsigned int status)
{
#define S(_x) case _x: return #_x
  switch (status)
  {
    S(ACX1_OK);
    S(ACX1_NO_CODE);
    S(ACX1_TERM_OPEN_FAILED);
    S(ACX1_TERM_IO_FAILED);
    S(ACX1_THREAD_ERROR);
    S(ACX1_NO_MEM);
    S(ACX1_CREATE_PIPE_ERROR);
    S(ACX1_WORKER_DIED);
    S(ACX1_SIGNAL_ERROR);
    S(ACX1_ALREADY_WRITING);
    S(ACX1_NOT_WRITING);
    S(ACX1_NOT_SUPPORTED);
  default:
    return "ACX1_UNSPECIFIED_STATUS";
  }
#undef S
}

/* acx1_key_name ************************************************************/
ACX1_API void * ACX1_CALL acx1_key_name (void * out, uint32_t km, int mode)
{
  char * o = out;

  (void) mode;
  if ((km & ACX1_CTRL))
  {
    strcpy(o, "Ctrl-");
    o += strlen(o);
  }
  if ((km & ACX1_ALT))
  {
    strcpy(o, "Alt-");
    o += strlen(o);
  }
  if ((km & ACX1_SHIFT))
  {
    strcpy(o, "Shift-");
    o += strlen(o);
  }
  km &= ACX1_KEY_MASK;
  if (km <= 0x20)
  {
    switch (km)
    {
    case ACX1_TAB:              strcpy(o, "Tab");               break;
    case ACX1_BACKSPACE:        strcpy(o, "Backspace");         break;
    case ACX1_ESC:              strcpy(o, "Escape");            break;
    case ACX1_ENTER:            strcpy(o, "Enter");             break;
    case ACX1_SPACE:            strcpy(o, "Space");             break;
    default:
      sprintf(o, "<BAD-0x%X>", km);
    }
    return out;
  }
  if (km < 0x7F)
  {
    *o++ = km;
    *o = 0;
    return out;
  }
  if (km < 0x110000)
  {
    sprintf(o, "<U+%06X>", km);
    return out;
  }
  switch (km)
  {
  case ACX1_UP                  : strcpy(o, "Up");              break;
  case ACX1_DOWN                : strcpy(o, "Down");            break;
  case ACX1_LEFT                : strcpy(o, "Left");            break;
  case ACX1_RIGHT               : strcpy(o, "Right");           break;
  case ACX1_HOME                : strcpy(o, "Home");            break;
  case ACX1_END                 : strcpy(o, "End");             break;
  case ACX1_PAGE_UP             : strcpy(o, "PageUp");          break;
  case ACX1_PAGE_DOWN           : strcpy(o, "PageDown");        break;
  case ACX1_INS                 : strcpy(o, "Insert");          break;
  case ACX1_DEL                 : strcpy(o, "Delete");          break;
  case ACX1_KP_ENTER            : strcpy(o, "KeypadEnter");     break;
  case ACX1_KP_ADD              : strcpy(o, "KeypadPlus");      break;
  case ACX1_KP_SUB              : strcpy(o, "KeypadMinus");     break;
  case ACX1_KP_MUL              : strcpy(o, "KeypadMultiply");  break;
  case ACX1_KP_DIV              : strcpy(o, "KeypadDivide");    break;
  case ACX1_KP_COMMA            : strcpy(o, "KeypadPoint");     break;
  case ACX1_KP_UP               : strcpy(o, "KeypadUp");        break;
  case ACX1_KP_DOWN             : strcpy(o, "KeypadDown");      break;
  case ACX1_KP_LEFT             : strcpy(o, "KeypadLeft");      break;
  case ACX1_KP_RIGHT            : strcpy(o, "KeypadRight");     break;
  case ACX1_KP_HOME             : strcpy(o, "KeypadHome");      break;
  case ACX1_KP_END              : strcpy(o, "KeypadEnd");       break;
  case ACX1_KP_PAGE_UP          : strcpy(o, "KeypadPageUp");    break;
  case ACX1_KP_PAGE_DOWN        : strcpy(o, "KeypadPageDown");  break;
  case ACX1_KP_INS              : strcpy(o, "KeypadInsert");    break;
  case ACX1_KP_DEL              : strcpy(o, "KeypadDelete");    break;
  case ACX1_KP_CENTER           : strcpy(o, "KeypadCenter");    break;
  default:
    if (km >= ACX1_F1 && km <= ACX1_F12) sprintf(o, "F%u", km - ACX1_F1 + 1);
    else sprintf(o, "<BAD-0x%X>", km);
  }

  return out;
}

/* escstr *******************************************************************/
char * escstr (char * out, size_t out_len, void const * buf, size_t len)
{
  unsigned char const * b;
  unsigned char const * e;
  char * o = out;
  b = buf;
  e = b + len;
  for (; b < e && out_len; ++b)
  {

    if (*b >= 0x20 && *b < 0x7F && *b != '\\' && *b != '"')
    {
      *o++ = *b;
      --out_len;
    }
    else
    {
      if (out_len < 4) break;
      sprintf(o, "\\x%02X", *b);
      o += 4;
      out_len -= 4;
    }
  }
  *o = 0;
  return out;
}

