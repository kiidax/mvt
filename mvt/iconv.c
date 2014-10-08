/* Multi-purpose Virtual Terminal
 * Copyright (C) 2012 Katsuya Iida
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <mvt/mvt.h>
#include "private.h"
#include "debug.h"

mvt_iconv_t
mvt_iconv_open (int utf8_to_ucs4)
{
    if (utf8_to_ucs4) {
        return (mvt_iconv_t)1;
    } else {
        return (mvt_iconv_t)2;
    }
}

int
mvt_iconv (mvt_iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft)
{
    uint8_t *inp = (uint8_t *)*inbuf;
    uint8_t *inend = inp + *inbytesleft;
    uint8_t *outp = (uint8_t *)*outbuf;
    uint8_t *outend = outp + *outbytesleft;
    int result = 0;

    if ((int)cd == 1)
    {
        while (inp < inend) {
            uint32_t ch = *inp;
            if (outp + 4 > outend) {
                result = MVT_E2BIG;
                break;
            }
            if ((ch & 0x80) == 0x00) {
                inp++;
            } else if ((ch & 0xf0) == 0xc0) {
                if (inp + 2 > inend) {
                    result = MVT_EINVAL;
                    break;
                }
                ch = (ch & 0x1f) << 6;
                inp++;
                ch += *inp++ & 0x3f;
            } else if ((ch & 0xf0) == 0xe0) {
                if (inp + 3 > inend) {
                    result = MVT_EINVAL;
                    break;
                }
                ch = (ch & 0x0f) << 12;
                inp++;
                ch += (*inp++ & 0x3f) << 6;
                ch += *inp++ & 0x3f;
            } else if ((ch & 0xf8) == 0xf0) {
                if (inp + 4 > inend) {
                    result = MVT_EINVAL;
                    break;
                }
                ch = (ch & 0x07) << 18;
                inp++;
                ch += (*inp++ & 0x3f) << 12;
                ch += (*inp++ & 0x3f) << 6;
                ch += *inp++ & 0x3f;
            } else if ((ch & 0xfc) == 0xf8) {
                if (inp + 5 > inend) {
                    result = MVT_EINVAL;
                    break;
                }
                ch = (ch & 0x03) << 24;
                inp++;
                ch += (*inp++ & 0x3f) << 18;
                ch += (*inp++ & 0x3f) << 12;
                ch += (*inp++ & 0x3f) << 6;
                ch += *inp++ & 0x3f;
            } else if ((ch & 0xfe) == 0xfc) {
                if (inp + 6 > inend) {
                    result = MVT_EINVAL;
                    break;
                }
                ch = (ch & 0x01) << 30;
                inp++;
                ch += (*inp++ & 0x3f) << 24;
                ch += (*inp++ & 0x3f) << 18;
                ch += (*inp++ & 0x3f) << 12;
                ch += (*inp++ & 0x3f) << 6;
                ch += *inp++ & 0x3f;
            } else {
                inp++;
            }
            *(uint32_t *)outp = ch;
            outp += 4;
        }
    } else if ((int)cd == 2) {
        while (TRUE) {
            uint32_t ch;

            if (inp + 4 > inend) {
                result = MVT_EINVAL;
                break;
            }

            ch = *(uint32_t *)inp;
            if (ch < 0x80) {
                if (outp + 1 > outend) {
                    result = MVT_E2BIG;
                    break;
                }
                *outp++ = (uint8_t)ch;
            } else if (ch < 0x800) {
                if (outp + 2 > outend) {
                    result = MVT_E2BIG;
                    break;
                }
                *outp++ = (uint8_t)((ch >> 6) & 0x1f | 0xc0);
                *outp++ = (uint8_t)(ch & 0x3f | 0x80);
            } else if (ch < 0x10000) {
                if (outp + 3 > outend) {
                    result = MVT_E2BIG;
                    break;
                }
                *outp++ = (uint8_t)((ch >> 12) & 0x0f | 0xe0);
                *outp++ = (uint8_t)((ch >> 6) & 0x3f | 0x80);
                *outp++ = (uint8_t)(ch & 0x3f | 0x80);
            } else if (ch < 0x200000) {
                if (outp + 4 > outend) {
                    result = MVT_E2BIG;
                    break;
                }
                *outp++ = (uint8_t)((ch >> 18) & 0x07 | 0xf0);
                *outp++ = (uint8_t)((ch >> 12) & 0x3f | 0x80);
                *outp++ = (uint8_t)((ch >> 6) & 0x3f | 0x80);
                *outp++ = (uint8_t)(ch & 0x3f | 0x80);
            } else if (ch < 0x4000000) {
                if (outp + 5 > outend) {
                    result = MVT_E2BIG;
                    break;
                }
                *outp++ = (uint8_t)((ch >> 24) & 0x03 | 0xf8);
                *outp++ = (uint8_t)((ch >> 18) & 0x3f | 0x80);
                *outp++ = (uint8_t)((ch >> 12) & 0x3f | 0x80);
                *outp++ = (uint8_t)((ch >> 6) & 0x3f | 0x80);
                *outp++ = (uint8_t)(ch & 0x3f | 0x80);
            } else if (ch < 0x80000000) {
                if (outp + 6 > outend) {
                    result = MVT_E2BIG;
                    break;
                }
                *outp++ = (uint8_t)((ch >> 30) & 0x01 | 0xfc);
                *outp++ = (uint8_t)((ch >> 24) & 0x3f | 0x80);
                *outp++ = (uint8_t)((ch >> 18) & 0x3f | 0x80);
                *outp++ = (uint8_t)((ch >> 12) & 0x3f | 0x80);
                *outp++ = (uint8_t)((ch >> 6) & 0x3f | 0x80);
                *outp++ = (uint8_t)(ch & 0x3f | 0x80);
            } else {
            }
            inp += 4;
        }
    } else {
        result = MVT_EILSEQ;
    }
    *inbuf = (char *)inp;
    *inbytesleft = inend - inp;
    *outbuf = (char *)outp;
    *outbytesleft = outend - outp;
    return result;
}

void
mvt_iconv_close (mvt_iconv_t cd)
{
}
