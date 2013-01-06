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
#include <acx1.h>

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

