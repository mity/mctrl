/*
 * mCtrl: Additional Win32 controls
 * <https://github.com/mity/mctrl>
 * <https://mctrl.org>
 *
 * Copyright (c) 2020 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "entity.h"


/* Note that because it is quite a lot of data (~20 kB), the table uses a
 * relatively compact representation where all records are concatenated into a
 * long flat buffer where:
 *
 * - The the entity names are stored without the '&' and ';'.
 *
 * - The Unicode counterparts of the entities (payload) are stored in a special
 *   encoding similar to UTF-8. Leading byte is always (0xc0 | SOMETHING), the
 *   trailing bytes are (0x80 | SOMETHING) where each SOMETHING encodes 6 bits
 *   from the Unicode codepoint (leading byte encodes the most significant bits
 *   of it).
 *
 * - There are no delimiters: We recognize what is what by knowing the entity
 *   names are in ASCII while the payload has the most significant bit always
 *   set.
 *
 * The output is sorted so that the lookup can be done via binary search.
 */
static const uint8_t entity_map[] = {
    /* Special sentinel value at the beginning.
     *
     * Together with zero-terminator at the end, this simplifies scanning for
     * beginning and end of each record so that we don't need to test whether
     * we reach beginning/end of the whole thing.
     *
     * It only means we have to limit the initial area of the binary search to
     * skip the sentinel. */
    "\xff"

    /* Preprocessor magic to concatenate all entity names and their respective
     * payloads in an alternating fashion into a single huge literal. */
    #define ENTITY_MAP_RECORD(name, utf8)        name utf8
    #include "entity_map.h"
    #undef ENTITY_MAP_RECORD
};


/* Only ASCII chars may form the entity name. */
#define IS_NAME_CHAR(type, ch)  (((type)(ch)) < 128  &&  (type)(ch) != (type)';')



#define IS_DIGIT(ch)    (_T('0') <= (ch) && (ch) <= _T('9'))
#define DIGIT_VAL(ch)   ((ch) - _T('0'))

#define IS_XDIGIT(ch)   ((_T('0') <= (ch) && (ch) <= _T('9'))  ||             \
                         (_T('a') <= (ch) && (ch) <= _T('f'))  ||             \
                         (_T('A') <= (ch) && (ch) <= _T('F')))
#define XDIGIT_VAL(ch)  ((_T('0') <= (ch) && (ch) <= _T('9'))                 \
                            ? (ch) - _T('0') :                                \
                         (_T('a') <= (ch) && (ch) <= _T('f'))                 \
                            ? 10 + (ch) - _T('a') : 10 + (ch) - _T('A'))

static BOOL
entity_is_valid_name(const TCHAR* ent_name)
{
    int i = 0;

    while(IS_NAME_CHAR(TCHAR, ent_name[i]))
        i++;
    return (i > 0  &&  ent_name[i] == _T(';'));
}

static int
entity_cmp(const TCHAR* ent_name, const uint8_t* map_name)
{
    int i = 0;
    BOOL ent_reached_str_end;
    BOOL map_reached_str_end;

    while(TRUE) {
        ent_reached_str_end = !IS_NAME_CHAR(TCHAR, ent_name[i]);
        map_reached_str_end = !IS_NAME_CHAR(uint8_t, map_name[i]);

        if(ent_reached_str_end || map_reached_str_end) {
            if(ent_reached_str_end && map_reached_str_end)
                return 0;
            else
                return (ent_reached_str_end ? -1 : +1);
        }

        if(ent_name[i] != (TCHAR) map_name[i])
            return (ent_name[i] - (TCHAR) map_name[i]);
        i++;
    }
}

static void
entity_out_unicode_codepoint(entity_t* ent, uint32_t codepoint)
{
    if(codepoint < 0x10000) {
        ent->buffer[ent->len++] = codepoint;
    } else {
        /* Break it into a surrogate pair. */
        ent->buffer[ent->len++] = 0xd800 | ((codepoint >> 10) & 0x3ff);
        ent->buffer[ent->len++] = 0xdc00 | ((codepoint >>  0) & 0x3ff);
    }
}

static void
entity_decode_payload(entity_t* ent, const uint8_t* payload)
{
    uint32_t c;
    int off = 0;

    while(TRUE) {
        c = payload[off] & 0x3f;
        off++;
        while((payload[off] & 0xc0) == 0x80) {
            c = (c << 6) | (payload[off] & 0x3f);
            off++;
        }

        entity_out_unicode_codepoint(ent, c);

        if((payload[off] & 0xc0) != 0xc0)
            break;
    }
}

int
entity_decode(const TCHAR* name, entity_t* ent)
{
    size_t beg = 1; /* 1 to skip the sentinel */
    size_t end = sizeof(entity_map);
    size_t pivot_beg;
    size_t pivot_end;
    int cmp;

    ent->len = 0;

    /* Numerical entity. */
    if(name[0] == '#') {
        uint32_t c = 0;
        int off;

        if((name[1] == _T('x') || name[1] == _T('X'))  &&  IS_XDIGIT(name[2])) {
            /* Hexadecimal entity (e.g. "&#x1234abcd;")). */
            for(off = 2; IS_XDIGIT(name[off]); off++)
                c = 16 * c + XDIGIT_VAL(name[off]);
        } else if(IS_DIGIT(name[1])) {
            /* Decimal entity (e.g. "&1234;") */
            for(off = 1; IS_DIGIT(name[off]); off++)
                c = 10 * c + DIGIT_VAL(name[off]);
        }

        MC_ASSERT(name[off] == _T(';'));
        entity_out_unicode_codepoint(ent, c);
        return 0;
    }

    /* Named HTML entity. */
    if(!entity_is_valid_name(name))
        return -1;

    while(beg < end) {
        /* Choose the pivot somewhere in the middle.
         *
         * Find beginning of the pivot record by iterating back until we reach
         * a boundary where non-ASCII byte precedes an ASCII byte.
         *
         * Find end of the pivot record by iterating forward until we reach
         * a boundary where ASCII byte (next entity name) follows a non-ASCII
         * byte. */
        pivot_beg = (end + beg) / 2;
        while(!IS_NAME_CHAR(uint8_t, entity_map[pivot_beg]) || IS_NAME_CHAR(uint8_t, entity_map[pivot_beg-1]))
            pivot_beg--;
        pivot_end = pivot_beg + 1;
        while(IS_NAME_CHAR(uint8_t, entity_map[pivot_end-1]) || !IS_NAME_CHAR(uint8_t, entity_map[pivot_end]))
            pivot_end++;

        cmp = entity_cmp(name, entity_map + pivot_beg);

        if(cmp < 0) {
            /* Continue the search in the area on the left from the pivot. */
            end = pivot_beg;
        } else if(cmp > 0) {
            /* Continue the search in the area ton the right from the pivot. */
            beg = pivot_end;
        } else {
            const uint8_t* payload;

            payload = entity_map + pivot_beg;
            while(IS_NAME_CHAR(uint8_t, *payload))
                payload++;

            entity_decode_payload(ent, payload);
            return 0;
        }
    }

    return -1;
}
