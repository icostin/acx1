/* acx1 - Application Console Interface - ver. 1
 */
#ifndef _ACX1_H_
#define _ACX1_H_

#include <stdio.h>
#include <limits.h>

/* int types ****************************************************************/
#if !ACX1_HAVE_INT_TYPES
# if _MSC_VER

#   include <stddef.h>
#   include <basetsd.h>

typedef unsigned char       uint8_t;
typedef   signed char        int8_t;
typedef unsigned short int  uint16_t;
typedef   signed short int   int16_t;
typedef unsigned int        uint32_t;
typedef   signed int         int32_t;
# else
#   include <stdint.h>
#   include <unistd.h>
# endif /* _MSC_VER / !_MSC_VER */
#endif /* !ACX1_HAVE_INT_TYPES */

#if !NO_UINT_T
typedef unsigned int uint_t;
#endif


/* public api function attributes *******************************************/
#if defined(_WIN32) || defined(__CYGWIN__)
# define ACX1_DL_EXPORT __declspec(dllexport)
# define ACX1_DL_IMPORT __declspec(dllimport)
# define ACX1_LOCAL
#elif __GNUC__ >= 4
# define ACX1_DL_IMPORT __attribute__ ((visibility ("default")))
# define ACX1_DL_EXPORT __attribute__ ((visibility ("default")))
# define ACX1_LOCAL __attribute__ ((visibility ("hidden")))
#elif defined(__GNUC__)
# define ACX1_DL_IMPORT
# define ACX1_DL_EXPORT
# define ACX1_LOCAL
#endif

#if ACX1_STATIC
# define ACX1_API
#elif ACX1_DLIB_BUILD
# define ACX1_API ACX1_DL_EXPORT
#else
# define ACX1_API ACX1_DL_IMPORT
#endif

#ifdef _WIN32
#define ACX1_CALL __fastcall
#elif __GNUC__ >= 3 && __amd64__
#define ACX1_CALL __attribute__((regparm(5)))
#elif __GNUC__ >= 3 && __i386__
#define ACX1_CALL __attribute__((regparm(3)))
#else
#define ACX1_CALL
#endif

#define ACX1_ITEM_COUNT(_a) (sizeof(_a) / sizeof(_a[0]))

/* status codes *************************************************************/
#define ACX1_OK                 0
#define ACX1_NO_CODE            1
#define ACX1_TERM_OPEN_FAILED   2
#define ACX1_TERM_IO_FAILED     3
#define ACX1_THREAD_ERROR       4
#define ACX1_NO_MEM             5
#define ACX1_CREATE_PIPE_ERROR  6
#define ACX1_WORKER_DIED        7
#define ACX1_SIGNAL_ERROR       8
#define ACX1_ALREADY_WRITING    9
#define ACX1_NOT_WRITING        10
#define ACX1_NOT_SUPPORTED      11
#define ACX1_BAD_DATA           12

/* keys *********************************************************************/
#define ACX1_KEY_MASK           0x003FFFFF

#define ACX1_TAB                0x00000009 /**< Tab. */
#define ACX1_BACKSPACE          0x00000008 /**< Backspace. */
#define ACX1_ESC                0x0000001B /**< Escape. */
#define ACX1_ENTER              0x0000000D /**< Enter. */
#define ACX1_SPACE              0x00000020 /**< Space. */
#define ACX1_UP                 0x00200000
#define ACX1_DOWN               0x00200001
#define ACX1_LEFT               0x00200002
#define ACX1_RIGHT              0x00200003
#define ACX1_HOME               0x00200004
#define ACX1_END                0x00200005
#define ACX1_PAGE_UP            0x00200006
#define ACX1_PAGE_DOWN          0x00200007
#define ACX1_INS                0x00200008
#define ACX1_DEL                0x00200009
#define ACX1_KP_ENTER           0x0020000A
#define ACX1_KP_ADD             0x0020000B
#define ACX1_KP_SUB             0x0020000C
#define ACX1_KP_MUL             0x0020000D
#define ACX1_KP_DIV             0x0020000E
#define ACX1_KP_COMMA           0x0020000F
#define ACX1_KP_UP              0x00200010
#define ACX1_KP_DOWN            0x00200011
#define ACX1_KP_LEFT            0x00200012
#define ACX1_KP_RIGHT           0x00200013
#define ACX1_KP_HOME            0x00200014
#define ACX1_KP_END             0x00200015
#define ACX1_KP_PAGE_UP         0x00200016
#define ACX1_KP_PAGE_DOWN       0x00200017
#define ACX1_KP_INS             0x00200018
#define ACX1_KP_DEL             0x00200019
#define ACX1_KP_CENTER          0x0020001A
#define ACX1_F(_x) (0x200020 | (_x))
#define ACX1_F1                 ACX1_F(1)
#define ACX1_F2                 ACX1_F(2)
#define ACX1_F3                 ACX1_F(3)
#define ACX1_F4                 ACX1_F(4)
#define ACX1_F5                 ACX1_F(5)
#define ACX1_F6                 ACX1_F(6)
#define ACX1_F7                 ACX1_F(7)
#define ACX1_F8                 ACX1_F(8)
#define ACX1_F9                 ACX1_F(9)
#define ACX1_F10                ACX1_F(10)
#define ACX1_F11                ACX1_F(11)
#define ACX1_F12                ACX1_F(12)

/* modifiers ****************************************************************/
#define ACX1_SHIFT              0x00400000
#define ACX1_ALT                0x00800000
#define ACX1_CTRL               0x01000000

/* colours ******************************************************************/
#define ACX1_BLACK              0
#define ACX1_RED                1
#define ACX1_GREEN              2
#define ACX1_YELLOW             3
#define ACX1_BLUE               4
#define ACX1_MAGENTA            5
#define ACX1_CYAN               6
#define ACX1_GRAY               7
#define ACX1_GREY               7

#define ACX1_DARK               0
#define ACX1_LIGHT              8

#define ACX1_DARK_RED           (1 | ACX1_DARK)
#define ACX1_DARK_GREEN         (2 | ACX1_DARK)
#define ACX1_DARK_YELLOW        (3 | ACX1_DARK)
#define ACX1_DARK_BLUE          (4 | ACX1_DARK)
#define ACX1_DARK_MAGENTA       (5 | ACX1_DARK)
#define ACX1_DARK_CYAN          (6 | ACX1_DARK)
#define ACX1_LIGHT_GRAY         (7 | ACX1_DARK)
#define ACX1_LIGHT_GREY         (7 | ACX1_DARK)

#define ACX1_DARK_GRAY          (0 | ACX1_LIGHT)
#define ACX1_DARK_GREY          (0 | ACX1_LIGHT)
#define ACX1_LIGHT_RED          (1 | ACX1_LIGHT)
#define ACX1_LIGHT_GREEN        (2 | ACX1_LIGHT)
#define ACX1_LIGHT_YELLOW       (3 | ACX1_LIGHT)
#define ACX1_LIGHT_BLUE         (4 | ACX1_LIGHT)
#define ACX1_LIGHT_MAGENTA      (5 | ACX1_LIGHT)
#define ACX1_LIGHT_CYAN         (6 | ACX1_LIGHT)
#define ACX1_WHITE              (7 | ACX1_LIGHT)

/* event_types **************************************************************/
#define ACX1_NONE               0
#define ACX1_RESIZE             1
#define ACX1_KEY                2
#define ACX1_ERROR              3
#define ACX1_FINISH             4

typedef struct acx1_event_s acx1_event_t;
struct acx1_event_s
{
  uint8_t type;
  union
  {
    uint32_t km; // unicode char / key with modifiers
    struct
    {
      uint16_t w, h;
    } size;
  };
};

#define ACX1_NORMAL             0
#define ACX1_BOLD               (1 << 0)
#define ACX1_UNDERLINE          (1 << 1)
#define ACX1_INVERSE            (1 << 2)


typedef struct acx1_attr_s acx1_attr_t;
struct acx1_attr_s
{
  uint32_t bg;
  uint32_t fg;
  uint32_t mode;
};


#ifdef __cplusplus
extern "C" {
#endif

ACX1_API char const * ACX1_CALL acx1_name ();
ACX1_API char const * ACX1_CALL acx1_status_str (unsigned int status);
ACX1_API void acx1_logging (int level, FILE * lf);
ACX1_API void * ACX1_CALL acx1_key_name (void * out, uint32_t km, int mode);
ACX1_API unsigned int ACX1_CALL acx1_init ();
ACX1_API void ACX1_CALL acx1_finish ();
ACX1_API unsigned int ACX1_CALL acx1_read_event (acx1_event_t * event_p);
ACX1_API unsigned int ACX1_CALL acx1_set_cursor_mode (uint8_t mode);
ACX1_API unsigned int ACX1_CALL acx1_get_cursor_mode (uint8_t * mode_p);
ACX1_API unsigned int ACX1_CALL acx1_set_cursor_pos (uint16_t r, uint16_t c);
ACX1_API unsigned int ACX1_CALL acx1_get_cursor_pos (uint16_t * r, uint16_t * c);
ACX1_API unsigned int ACX1_CALL acx1_get_screen_size (uint16_t * h, uint16_t * w);
ACX1_API unsigned int ACX1_CALL acx1_write_start ();
ACX1_API unsigned int ACX1_CALL acx1_charset (unsigned int cs);
ACX1_API unsigned int ACX1_CALL acx1_attr (int bg, int fg, unsigned int mode);
ACX1_API unsigned int ACX1_CALL acx1_write_pos (uint16_t r, uint16_t c);
ACX1_API unsigned int ACX1_CALL acx1_write (void const * data, size_t len);
ACX1_API unsigned int ACX1_CALL acx1_fill (uint32_t ch, uint16_t count);
ACX1_API unsigned int ACX1_CALL acx1_clear ();
ACX1_API unsigned int ACX1_CALL acx1_write_stop ();

ACX1_API unsigned int ACX1_CALL acx1_rect
(
  uint8_t const * const * data, // array of rows of utf8 text with special escapes
  uint16_t start_row,
  uint16_t start_col,
  uint16_t row_num, 
  uint16_t col_num,
  acx1_attr_t * attrs
);

ACX1_API void * ACX1_CALL acx1_hexz (void * out, void const * in, size_t len);
ACX1_API int ACX1_CALL acx1_utf8_char_decode_strict
(
  void const * vdata,
  size_t len,
  uint32_t * out
);

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
);

ACX1_API int ACX1_CALL acx1_term_char_width_wctx (uint32_t cp, void * ctx);

ACX1_API int ACX1_CALL acx1_mutf8_str_decode
(
  void const * vdata,
  size_t len,
  uint16_t * out_a,
  size_t out_n,
  size_t * in_len_p,
  size_t * out_len_p
);

ACX1_API int ACX1_CALL acx1_term_char_width (uint32_t cp);

#ifdef __cplusplus
};
#endif

#endif /* _ACX1_H_ */

