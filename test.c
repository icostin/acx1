/* acx1 - Application Console Interface - ver. 1
 *
 * Silly test program for library acx1
 *
 * Changelog:
 *  - 2013/01/06 Costin Ionescu: initial release
 *
 */
#include <string.h>
#include <stdio.h>
#include <acx1.h>

#define C(_expr) if ((rc = (_expr))) do { line = __LINE__; goto l_exit; } while (0)

int main (int argc, char const * const * argv)
{
  acx1_event_t e;
  unsigned int rc;
  uint16_t h, w, r, c;
  char name[0x40];
  char buf[0x80];
  int line;
  acx1_attr_t rect_attrs[3] =
  {
    { ACX1_BLUE, ACX1_LIGHT_YELLOW, 0 },
    { ACX1_GREEN, ACX1_BLACK, ACX1_BOLD },
    { ACX1_BLACK, ACX1_LIGHT_RED, ACX1_UNDERLINE },
  };
  uint8_t const * (rect_lines[]) =
  {
    (uint8_t const *) "Abcd " "\a\x01" "def" "\a\x02" "ghi",
    (uint8_t const *) "aaaaaaaaaaaaaaa\a\x01" "aaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  };

  printf("acx1 test\n"
         "- using dynamic library: %s\n", acx1_name());
  if (argc > 1)
  {
    printf("- arg1: %s\n", argv[1]);
  }

  acx1_logging(3, stderr);
  C(acx1_init());

  C(acx1_get_screen_size(&h, &w));
  printf("- screen size: %ux%u\n\r", w, h);
  C(acx1_get_cursor_pos(&r, &c));
  printf("- cursor pos: %ux%u\n\r", c, r);
  C(acx1_set_cursor_pos(2, 2));
  sprintf(buf, "reading events until 'q' is pressed "
         "or something goes wrong...");
  for (;;)
  {
    C(acx1_write_start());
    C(acx1_write_pos(h, 1));
    C(acx1_attr(0, 7, 0));
    C(acx1_write("[ ", 2));
    C(acx1_attr(4, 6, ACX1_BOLD));
    C(acx1_write(buf, strlen(buf)));
    C(acx1_attr(0, 7, 0));
    C(acx1_write(" ]", 2));
    C(acx1_fill('-', w - 4 - strlen(buf)));
    C(acx1_write_pos(r, c));
    C(acx1_attr(4, 11, 0));
    C(acx1_write("*", 1));
    C(acx1_write_stop());
    C(acx1_read_event(&e));
    switch (e.type)
    {
    case ACX1_KEY:
      sprintf(buf, "key: km = 0x%08X name = %s",
             e.km, (char *) acx1_key_name(name, e.km, 0));
      if (e.km == (ACX1_CTRL | 'L'))
      {
        C(acx1_write_start());
        C(acx1_attr(0, 7, 0));
        C(acx1_clear());
        C(acx1_write_stop());
      }
      if (e.km == 'q') goto l_exit;
      if (e.km == ACX1_UP)
      {
        r -= 1;
        if (!r) r = 1;
        C(acx1_set_cursor_pos(r, c));
      }
      if (e.km == ACX1_DOWN)
      {
        r += 1;
        if (r >= h) r = h;
        C(acx1_set_cursor_pos(r, c));
      }
      if (e.km == ACX1_LEFT)
      {
        c -= 1;
        if (!c) c = 1;
        C(acx1_set_cursor_pos(r, c));
      }
      if (e.km == ACX1_RIGHT)
      {
        c += 1;
        if (c >= w) c = w;
        C(acx1_set_cursor_pos(r, c));
      }
      if (e.km == 'C') C(acx1_set_cursor_mode(0));
      if (e.km == 'c') C(acx1_set_cursor_mode(1));
      if (e.km == 'r') C(acx1_rect(rect_lines, r, c, 2, 16, rect_attrs));
      break;
    case ACX1_RESIZE:
      sprintf(buf, "screen resized: %ux%u", e.size.w, e.size.h);
      w = e.size.w;
      h = e.size.h;
      break;
    case ACX1_ERROR:
      sprintf(buf, "error event");
      goto l_exit;
    default:
      sprintf(buf, "event.type=0x%X", e.type);
      goto l_exit;
    }
  }

l_exit:
  acx1_finish();

  if (rc)
  {
    fprintf(stderr, "Error: %s (code=%u, line=%u)\n", acx1_status_str(rc), rc, line);
  }

  return rc;
}

