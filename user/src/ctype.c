#include "ctype.h"

/*
 * Character classification lookup table
 * Indexed by character value (0-255)
 * 
 * Flags:
 *   _CTYPE_U  (0x01) - Upper case (A-Z)
 *   _CTYPE_L  (0x02) - Lower case (a-z)
 *   _CTYPE_D  (0x04) - Digit (0-9)
 *   _CTYPE_S  (0x08) - Space (space, \t, \n, \v, \f, \r)
 *   _CTYPE_P  (0x10) - Punctuation
 *   _CTYPE_C  (0x20) - Control character
 *   _CTYPE_X  (0x40) - Hex letter (A-F, a-f, not 0-9)
 *   _CTYPE_B  (0x80) - Blank (space, \t)
 */

const unsigned char __ctype_table[256] = {
    [0x00] = _CTYPE_C,                      /* NUL */
    [0x01] = _CTYPE_C,                      /* SOH */
    [0x02] = _CTYPE_C,                      /* STX */
    [0x03] = _CTYPE_C,                      /* ETX */
    [0x04] = _CTYPE_C,                      /* EOT */
    [0x05] = _CTYPE_C,                      /* ENQ */
    [0x06] = _CTYPE_C,                      /* ACK */
    [0x07] = _CTYPE_C,                      /* BEL */
    [0x08] = _CTYPE_C,                      /* BS  */
    [0x09] = _CTYPE_C | _CTYPE_S | _CTYPE_B,/* HT (tab) - control, space, blank */
    [0x0A] = _CTYPE_C | _CTYPE_S,           /* LF - control, space */
    [0x0B] = _CTYPE_C | _CTYPE_S,           /* VT - control, space */
    [0x0C] = _CTYPE_C | _CTYPE_S,           /* FF - control, space */
    [0x0D] = _CTYPE_C | _CTYPE_S,           /* CR - control, space */
    [0x0E] = _CTYPE_C,                      /* SO  */
    [0x0F] = _CTYPE_C,                      /* SI  */
    [0x10] = _CTYPE_C,                      /* DLE */
    [0x11] = _CTYPE_C,                      /* DC1 */
    [0x12] = _CTYPE_C,                      /* DC2 */
    [0x13] = _CTYPE_C,                      /* DC3 */
    [0x14] = _CTYPE_C,                      /* DC4 */
    [0x15] = _CTYPE_C,                      /* NAK */
    [0x16] = _CTYPE_C,                      /* SYN */
    [0x17] = _CTYPE_C,                      /* ETB */
    [0x18] = _CTYPE_C,                      /* CAN */
    [0x19] = _CTYPE_C,                      /* EM  */
    [0x1A] = _CTYPE_C,                      /* SUB */
    [0x1B] = _CTYPE_C,                      /* ESC */
    [0x1C] = _CTYPE_C,                      /* FS  */
    [0x1D] = _CTYPE_C,                      /* GS  */
    [0x1E] = _CTYPE_C,                      /* RS  */
    [0x1F] = _CTYPE_C,                      /* US  */

    [' ']  = _CTYPE_S | _CTYPE_B,
    ['!']  = _CTYPE_P,
    ['"']  = _CTYPE_P,
    ['#']  = _CTYPE_P,
    ['$']  = _CTYPE_P,
    ['%']  = _CTYPE_P,
    ['&']  = _CTYPE_P,
    ['\''] = _CTYPE_P,
    ['(']  = _CTYPE_P,
    [')']  = _CTYPE_P,
    ['*']  = _CTYPE_P,
    ['+']  = _CTYPE_P,
    [',']  = _CTYPE_P,
    ['-']  = _CTYPE_P,
    ['.']  = _CTYPE_P,
    ['/']  = _CTYPE_P,

    ['0']  = _CTYPE_D,
    ['1']  = _CTYPE_D,
    ['2']  = _CTYPE_D,
    ['3']  = _CTYPE_D,
    ['4']  = _CTYPE_D,
    ['5']  = _CTYPE_D,
    ['6']  = _CTYPE_D,
    ['7']  = _CTYPE_D,
    ['8']  = _CTYPE_D,
    ['9']  = _CTYPE_D,

    [':']  = _CTYPE_P,
    [';']  = _CTYPE_P,
    ['<']  = _CTYPE_P,
    ['=']  = _CTYPE_P,
    ['>']  = _CTYPE_P,
    ['?']  = _CTYPE_P,
    ['@']  = _CTYPE_P,

    ['A']  = _CTYPE_U | _CTYPE_X,
    ['B']  = _CTYPE_U | _CTYPE_X,
    ['C']  = _CTYPE_U | _CTYPE_X,
    ['D']  = _CTYPE_U | _CTYPE_X,
    ['E']  = _CTYPE_U | _CTYPE_X,
    ['F']  = _CTYPE_U | _CTYPE_X,
    ['G']  = _CTYPE_U,
    ['H']  = _CTYPE_U,
    ['I']  = _CTYPE_U,
    ['J']  = _CTYPE_U,
    ['K']  = _CTYPE_U,
    ['L']  = _CTYPE_U,
    ['M']  = _CTYPE_U,
    ['N']  = _CTYPE_U,
    ['O']  = _CTYPE_U,
    ['P']  = _CTYPE_U,
    ['Q']  = _CTYPE_U,
    ['R']  = _CTYPE_U,
    ['S']  = _CTYPE_U,
    ['T']  = _CTYPE_U,
    ['U']  = _CTYPE_U,
    ['V']  = _CTYPE_U,
    ['W']  = _CTYPE_U,
    ['X']  = _CTYPE_U,
    ['Y']  = _CTYPE_U,
    ['Z']  = _CTYPE_U,

    ['[']  = _CTYPE_P,
    ['\\'] = _CTYPE_P,
    [']']  = _CTYPE_P,
    ['^']  = _CTYPE_P,
    ['_']  = _CTYPE_P,
    ['`']  = _CTYPE_P,

    ['a']  = _CTYPE_L | _CTYPE_X,
    ['b']  = _CTYPE_L | _CTYPE_X,
    ['c']  = _CTYPE_L | _CTYPE_X,
    ['d']  = _CTYPE_L | _CTYPE_X,
    ['e']  = _CTYPE_L | _CTYPE_X,
    ['f']  = _CTYPE_L | _CTYPE_X,
    ['g']  = _CTYPE_L,
    ['h']  = _CTYPE_L,
    ['i']  = _CTYPE_L,
    ['j']  = _CTYPE_L,
    ['k']  = _CTYPE_L,
    ['l']  = _CTYPE_L,
    ['m']  = _CTYPE_L,
    ['n']  = _CTYPE_L,
    ['o']  = _CTYPE_L,
    ['p']  = _CTYPE_L,
    ['q']  = _CTYPE_L,
    ['r']  = _CTYPE_L,
    ['s']  = _CTYPE_L,
    ['t']  = _CTYPE_L,
    ['u']  = _CTYPE_L,
    ['v']  = _CTYPE_L,
    ['w']  = _CTYPE_L,
    ['x']  = _CTYPE_L,
    ['y']  = _CTYPE_L,
    ['z']  = _CTYPE_L,

    ['{']  = _CTYPE_P,
    ['|']  = _CTYPE_P,
    ['}']  = _CTYPE_P,
    ['~']  = _CTYPE_P,

    [0x7F] = _CTYPE_C,

    //0x80-0xFF: Extended ASCII - no classification (all zeros by default)
};