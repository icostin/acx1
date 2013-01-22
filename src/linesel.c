/* acx1 - Application Console Interface - ver. 1
 *
 * Tool to select a single line from some input text.
 * The text must not contain non-printable Unicode characters
 * (that includes TAB, which are evil anyway).
 *
 * This program can be used to test acx1 library.
 *
 * Changelog:
 *  - 2013/01/06 Costin Ionescu: initial release
 *
 */
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <acx1.h>
#include <c41.h>

int normal_bg = 4, normal_fg = 7;
int sel_bg = 7, sel_fg = 0;

FILE * log_file = NULL;

#define A(_expr) if (!(rc = (_expr))) ; else \
                    do { line = __LINE__; goto l_acx_fail; } while (0)

int aw (char * str)
{
  return acx1_write(str, strlen(str));
}

void * strstrci (char const * a, char const * b)
{
  char ac, bc;
  char const * ae;
  char const * aa;
  char const * bb;
  size_t al, bl;
  al = strlen(a);
  bl = strlen(b);
  if (al < bl) return NULL;
  for (ae = a + al - bl; a <= ae; ++a)
  {
    for (aa = a, bb = b; *bb; ++aa, ++bb)
    {
      ac = *aa;
      bc = *bb;
      if (ac == bc) continue;
      if ((ac ^ bc) != 0x20) break;
      ac |= 0x20;
      if (ac < 'a' || ac > 'z') break;
    }
    if (!*bb) return (void *) a;
  }
  return NULL;
}

int linesel (char * * v, int n, char * init_str)
{
  char ibuf[0x100];
  char obuf[0x100];
  acx1_event_t e;
  uint16_t w, h, r;
  int line, rc, st;
  int i, opt_lines, first, crt, nleft, c, ilen, ipos;
  int * xmap;
  char ichg;

  xmap = malloc(n * sizeof(int));
  A(!xmap);
  // if (!xmap) return -2;
  ichg = 1;
  for (i = 0; i < n; ++i) xmap[i] = i;

  ibuf[sizeof(ibuf) - 1] = 0;
  strncpy(ibuf, init_str, sizeof(ibuf) - 1);
  ilen = strlen(ibuf);
  ipos = ilen;

  A(acx1_get_screen_size(&h, &w));
  A(acx1_set_cursor_pos(h, 1));
  nleft = n;
  first = crt = 0;
  A(acx1_write_start());
  A(acx1_attr(0, 7, 0));
  A(acx1_clear());
  A(acx1_write_stop());
  for (;;)
  {
    opt_lines = h - 3;
    A(opt_lines <= 0);

//    if (opt_lines <= 0)
//    {
//      // screen too small; wait for resize
//      return -2;
//    }

    if (ichg)
    {
      ichg = 0;
      first = xmap[first];
      crt = xmap[crt];
      for (nleft = i = 0; i < n; ++i)
        if (strstrci(v[i], ibuf)) xmap[nleft++] = i;
      for (i = 0; i < nleft && first > xmap[i]; ++i);
      first = i ? i - 1 : 0;
      for (i = 0; i < nleft && crt > xmap[i]; ++i);
      if (i == nleft) --i;
      if (!i || crt == xmap[i]) crt = i;
      else crt = i - 1;
    }

    if (first + opt_lines <= crt) first = crt + 1 - opt_lines;
    if (first + opt_lines > nleft) first = nleft - opt_lines;
    if (first < 0) first = 0;
    if (crt < first) first = crt;

    A(acx1_write_start());
    r = nleft > opt_lines ? 1 : 1 + opt_lines - nleft;
    for (i = 1; i < r; ++i)
    {
      A(acx1_write_pos(i, 1));
      A(acx1_fill(' ', w));
    }

    for (i = first; r <= opt_lines; ++i, ++r)
    {
      size_t bpar, cpar, wpar;

      A(acx1_write_pos(r, 1));
      c = strlen(v[xmap[i]]);

      st = c41_utf8_str_measure(c41_term_char_width_wctx, NULL,
                                v[xmap[i]], c, C41_SSIZE_MAX, w - 2,
                                &bpar, &cpar, &wpar);
      if (st < 0)
      {
        A(acx1_attr(1, 9, 0));
        A(aw("BAD UTF8 string!"));
        c = strlen("BAD UTF8 string!");
        wpar = c;
        bpar = 0;
      }

      if (crt != i) { A(acx1_attr(normal_bg, normal_fg, 0)); }
      else { A(acx1_attr(sel_bg, sel_fg, 0)); }

      // if (c > w) c = w;
      A(acx1_write(v[xmap[i]], bpar));
      if (wpar + 2 < w) { A(acx1_fill(' ', w - wpar - 2)); }
      A(acx1_attr(2, 7, 0));
      A(aw("| "));
    }
    A(acx1_write_pos(opt_lines + 1, 1));
    A(acx1_attr(0, 11, 0));
    A(aw("Filter text: "));
    A(acx1_attr(0, 10, 0));
    A(aw(ibuf));
    A(acx1_attr(0, 7, 0));
    c = strlen("Filter text: ") + ilen + 1;
    if (c > w) c = w;
    A(acx1_fill(' ', w - c - 1));
    A(acx1_write_pos(opt_lines + 2, 1));
    A(acx1_attr(0, 6, 0));
    r = nleft - first;
    if (r > opt_lines) r = opt_lines;
    if (nleft)
      sprintf(obuf, "%u option%s: %u filtered, %u available "
              "(%u above, %u displayed, %u below)",
              n, n == 1 ? "" : "s", n - nleft, nleft, first, 
              r, nleft - first - r);
    else sprintf(obuf, "%u option%s: all filtered, none available",
                 n, n == 1 ? "" : "s");
    c = strlen(obuf);
    A(acx1_write(obuf, c));
    A(acx1_fill(' ', w - c));
    A(acx1_write_stop());
    A(acx1_set_cursor_pos(opt_lines + 1, strlen("Filter text: ") + ipos + 1));

    A(acx1_read_event(&e));
    if (e.type == ACX1_RESIZE)
    {
      w = e.size.w;
      h = e.size.h;
      continue;
    }
    if (e.type != ACX1_KEY) return -2;
    switch (e.km)
    {
    case ACX1_ESC: 
    case ACX1_ALT | 'q': 
    case ACX1_ALT | 'x': 
    case ACX1_CTRL | 'Q': 
    case ACX1_CTRL | 'X': 
      return -1;
    case ACX1_UP:
    case ACX1_ALT | 'k':
      if (crt > 0) crt -= 1;
      break;
    case ACX1_DOWN:
    case ACX1_ALT | 'j':
      if (crt < nleft - 1) crt += 1;
      break;
    case ACX1_LEFT:
    case ACX1_ALT | 'h':
      if (ipos) ipos -= 1;
      break;
    case ACX1_RIGHT:
    case ACX1_ALT | 'l':
      if (ipos < ilen) ipos += 1;
      break;
    case ACX1_PAGE_UP:
    case ACX1_CTRL | 'B':
      crt -= opt_lines - 1;
      if (crt < 0) crt = 0;
      break;
    case ACX1_PAGE_DOWN:
    case ACX1_CTRL | 'F':
      crt += opt_lines - 1;
      if (crt >= nleft) crt = nleft - 1;
      first += opt_lines - 1;
      if (first >= nleft - opt_lines) first = nleft - opt_lines;
      break;
    case ACX1_CTRL | ACX1_PAGE_UP:
    case ACX1_ALT | 'U':
      crt = 0;
      break;
    case ACX1_CTRL | ACX1_PAGE_DOWN:
    case ACX1_ALT | 'D':
      crt = nleft - 1;
      break;
    case ACX1_ENTER:
      if (!nleft) break;
      return xmap[crt];
    case ACX1_ALT | 'H':
      crt = first;
      break;
    case ACX1_ALT | 'M':
      i = first + opt_lines - 1;
      if (i >= nleft) i = nleft - 1;
      // crt = first + opt_lines / 2;
      crt = (first + i) / 2;
      if (crt >= nleft) crt = nleft - 1;
      break;
    case ACX1_ALT | 'L':
      crt = first + opt_lines - 1;
      if (crt >= nleft) crt = nleft - 1;
      break;
    case ACX1_ALT | 'd':
      crt = crt + opt_lines / 4;
      if (crt >= nleft) crt = nleft - 1;
      // if (first >= nleft - opt_lines) first = nleft - opt_lines;
      // if (first < 0) first = 0;
      break;
    case ACX1_ALT | 'u':
      crt = crt - opt_lines / 4;
      if (crt < 0) crt = 0;
      break;
    case ACX1_CTRL | 'U':
      if (!ipos) break;
      if (ipos < ilen)
      {
        memmove(ibuf, &ibuf[ipos], ilen - ipos);
      }
      ilen -= ipos;
      ibuf[ilen] = 0;
      ipos = 0;
      ichg = 1;
      break;
    case ACX1_CTRL | ACX1_BACKSPACE:
    case ACX1_BACKSPACE:
      if (!ipos) break;
      --ipos;
      memmove(&ibuf[ipos], &ibuf[ipos + 1], ilen - ipos);
      --ilen;
      ichg = 1;
      break;
    }
    if (e.km >= 0x20 && e.km <= 0x7E)
    {
      if (ilen == sizeof(ibuf) - 1) continue;
      if (ipos < ilen)
      {
        memmove(&ibuf[ipos + 1], &ibuf[ipos], ilen - ipos);
      }
      ibuf[ipos] = e.km;
      ipos += 1;
      ilen += 1;
      ibuf[ilen] = 0;
      ichg = 1;
    }
  }

l_acx_fail:
  {
    FILE * f = log_file ? log_file : stderr;
    fprintf(f, "Error: %s (line %u)\n", acx1_status_str(rc), line);
  }
  return -2;
}

int main (int argc, char * * argv)
{
  unsigned int rc, line = 0;
  static char lbuf[0x10002];
  int i, l, n, a;
  char * * v;
  uint16_t w, h;

  if (argc == 2 && !strcmp(argv[1], "-h"))
  {
    printf(
      "Usage: linesel [-h] [-l ACX_LOG] < options.lst\n"
      "Synopsis:  asks user to choose one of the options from standard input\n"
      "           and prints that to standard output\n");
    return 0;
  }
  if (argc == 3 && !strcmp(argv[1], "-l"))
  {
    log_file = fopen(argv[2], "wt");
  }

  if (log_file) acx1_logging(3, log_file);

  i = 0; n = 0;
  a = 0x2;
  v = malloc(a * sizeof(char *));
  if (!v) goto l_no_mem;

  while (fgets(lbuf, sizeof(lbuf), stdin))
  {
    ++i;
    l = strlen(lbuf);
    if (l > 0x10000)
    {
      fprintf(stderr, "Error: line %u is too long (exceeds 64KB)\n", i);
      return 2;
    }
    if (lbuf[l - 1] == '\n') lbuf[--l] = 0;
    if (!l) continue;
    if (n == a)
    {
      a <<= 1;
      v = realloc(v, a * sizeof(char *));
      if (!v) goto l_no_mem;
    }
    v[n] = strdup(lbuf);
    if (!v[n]) goto l_no_mem;
    ++n;
  }

  if (ferror(stdin))
  {
    fprintf(stderr, "Error: failed reading input (%s, code %u)\n",
            strerror(errno), errno);
    return 2;
  }

  A(acx1_init());
  i = linesel(v, n, "");
  A(acx1_write_start());
  A(acx1_attr(0, 7, 0));
  A(acx1_clear());
  A(acx1_write_stop());
  A(acx1_get_screen_size(&h, &w));
  A(acx1_set_cursor_pos(h, 1));
  acx1_finish();

  if (i < 0)
  {
    fprintf(stderr, "linesel ret code: %d\n", i);
    return 1;
  }
  puts(v[i]);
  return 0;
l_acx_fail:
  acx1_finish();
  fprintf(stderr, "Error: %s (line %u)\n", acx1_status_str(rc), line);
  return 2;
l_no_mem:
  fprintf(stderr, "Error: not enough memory\n");
  return 2;
}

