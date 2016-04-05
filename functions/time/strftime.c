/* strftime( char * restrict, size_t, const char * restrict, const struct tm * restrict )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <time.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <assert.h>

#ifndef REGTEST

/* TODO: Alternative representations / numerals not supported. Multibyte support missing. */

/* This implementation's code is highly repetitive, but I did not really
   care for putting it into a number of macros / helper functions.
*/

enum wstart_t
{
    E_SUNDAY = 0,
    E_MONDAY = 1
};

#include <stdio.h>

static int weeknr( const struct tm * timeptr, int wstart )
{
    int wday = ( timeptr->tm_wday + 7 - wstart ) % 7;
    div_t week = div( timeptr->tm_yday, 7 );
    if ( week.rem >= wday )
    {
        ++week.quot;
    }
    return week.quot;
}

static int iso_week( const struct tm * timeptr )
{
    /* calculations below rely on Sunday == 7 */
    int wday = timeptr->tm_wday;
    if ( wday == 0 )
    {
        wday = 7;
    }
    /* https://en.wikipedia.org/wiki/ISO_week_date */
    int week = ( timeptr->tm_yday - wday + 11 ) / 7;
    if ( week == 53 )
    {
        /* date *may* belong to the *next* year, if:
           * it is 31.12. and Monday - Wednesday
           * it is 30.12. and Monday - Tuesday
           * it is 29.12. and Monday
           We can safely assume December...
        */
        if ( ( timeptr->tm_yday - wday - _PDCLIB_is_leap( timeptr->tm_year ) ) > 360 )
        {
            week = 1;
        }
    }
    else if ( week == 0 )
    {
        /* date *does* belong to *previous* year,
           i.e. has week 52 *unless*...
           * current year started on a Friday, or
           * previous year is leap and this year
             started on a Saturday.
        */
        int firstday = timeptr->tm_wday - ( timeptr->tm_yday % 7 );
        if ( firstday < 0 )
        {
            firstday += 7;
        }
        if ( ( firstday == 5 ) || ( _PDCLIB_is_leap( timeptr->tm_year - 1 ) && firstday == 6 ) )
        {
            week = 53;
        }
        else
        {
            week = 52;
        }
    }
    return week;
}

/* Assuming presence of s, rc, maxsize.
   Checks index for valid range, target buffer for sufficient remaining
   capacity, and copies the locale-specific string (or "?" if index out
   of range). Returns with zero if buffer capacity insufficient.
*/
#define SPRINTSTR( array, index, max ) \
    { \
        const char * str = "?"; \
        if ( index >= 0 && index <= max ) \
        { \
            str = array[ index ]; \
        } \
        size_t len = strlen( str ); \
        if ( rc < ( maxsize - len ) ) \
        { \
            strcpy( s + rc, str ); \
            rc += len; \
        } \
        else \
        { \
            return 0; \
        } \
    }

size_t strftime( char * _PDCLIB_restrict s, size_t maxsize, const char * _PDCLIB_restrict format, const struct tm * _PDCLIB_restrict timeptr )
{
    size_t rc = 0;

    while ( rc < maxsize )
    {
        if ( *format != '%' )
        {
            if ( ( s[rc] = *format++ ) == '\0' )
            {
                return rc;
            }
            else
            {
                ++rc;
            }
        }
        else
        {
            /* char flag = 0; */
            switch ( *++format )
            {
                case 'E':
                case 'O':
                    /* flag = *format++; */
                    break;
                default:
                    /* EMPTY */
                    break;
            }
            switch( *format++ )
            {
                case 'a':
                    {
                        /* tm_wday abbreviated */
                        SPRINTSTR( _PDCLIB_lconv.day_name_abbr, timeptr->tm_wday, 6 );
                        break;
                    }
                case 'A':
                    {
                        /* tm_wday full */
                        SPRINTSTR( _PDCLIB_lconv.day_name_full, timeptr->tm_wday, 6 );
                        break;
                    }
                case 'b':
                case 'h':
                    {
                        /* tm_mon abbreviated */
                        SPRINTSTR( _PDCLIB_lconv.month_name_abbr, timeptr->tm_mon, 11 );
                        break;
                    }
                case 'B':
                    {
                        /* tm_mon full */
                        SPRINTSTR( _PDCLIB_lconv.month_name_full, timeptr->tm_mon, 11 );
                        break;
                    }
                case 'c':
                    {
                        /* locale's date / time representation, %a %b %e %T %Y for C locale */
                        /* 'E' for locale's alternative representation */
                        size_t count = strftime( s + rc, maxsize - rc, _PDCLIB_lconv.date_time_format, timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'C':
                    {
                        /* tm_year divided by 100, truncated to decimal (00-99) */
                        /* 'E' for base year (period) in locale's alternative representation */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t period = div( ( ( timeptr->tm_year + 1900 ) / 100 ), 10 );
                            s[rc++] = '0' + period.quot;
                            s[rc++] = '0' + period.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'd':
                    {
                        /* tm_mday as decimal (01-31) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t day = div( timeptr->tm_mday, 10 );
                            s[rc++] = '0' + day.quot;
                            s[rc++] = '0' + day.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'D':
                    {
                        /* %m/%d/%y */
                        size_t count = strftime( s + rc, maxsize - rc, "%m/%d/%y", timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'e':
                    {
                        /* tm_mday as decimal ( 1-31) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t day = div( timeptr->tm_mday, 10 );
                            s[rc++] = ( day.quot > 0 ) ? '0' + day.quot : ' ';
                            s[rc++] = '0' + day.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'F':
                    {
                        /* %Y-%m-%d */
                        size_t count = strftime( s + rc, maxsize - rc, "%Y-%m-%d", timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'g':
                    {
                        /* last 2 digits of the week-based year as decimal (00-99) */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            int week = iso_week( timeptr );
                            int bias = 0;
                            if ( week >= 52 && timeptr->tm_mon == 0 )
                            {
                                --bias;
                            }
                            else if ( week == 1 && timeptr->tm_mon == 11 )
                            {
                                ++bias;
                            }
                            div_t year = div( timeptr->tm_year % 100 + bias, 10 );
                            s[rc++] = '0' + year.quot;
                            s[rc++] = '0' + year.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'G':
                    {
                        /* week-based year as decimal (e.g. 1997) */
                        if ( rc < ( maxsize - 4 ) )
                        {
                            int week = iso_week( timeptr );
                            int year = timeptr->tm_year + 1900;
                            if ( week >= 52 && timeptr->tm_mon == 0 )
                            {
                                --year;
                            }
                            else if ( week == 1 && timeptr->tm_mon == 11 )
                            {
                                ++year;
                            }
                            for ( int i = 3; i >= 0; --i )
                            {
                                div_t digit = div( year, 10 );
                                s[ rc + i ] = '0' + digit.rem;
                                year = digit.quot;
                            }

                            rc += 4;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'H':
                    {
                        /* tm_hour as 24h decimal (00-23) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t hour = div( timeptr->tm_hour, 10 );
                            s[rc++] = '0' + hour.quot;
                            s[rc++] = '0' + hour.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'I':
                    {
                        /* tm_hour as 12h decimal (01-12) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t hour = div( ( timeptr->tm_hour + 11 ) % 12 + 1, 10 );
                            s[rc++] = '0' + hour.quot;
                            s[rc++] = '0' + hour.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'j':
                    {
                        /* tm_yday as decimal (001-366) */
                        if ( rc < ( maxsize - 3 ) )
                        {
                            div_t yday = div( timeptr->tm_yday + 1, 100 );
                            s[rc++] = '0' + yday.quot;
                            s[rc++] = '0' + yday.rem / 10;
                            s[rc++] = '0' + yday.rem % 10;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'm':
                    {
                        /* tm_mon as decimal (01-12) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t mon = div( timeptr->tm_mon + 1, 10 );
                            s[rc++] = '0' + mon.quot;
                            s[rc++] = '0' + mon.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'M':
                    {
                        /* tm_min as decimal (00-59) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t min = div( timeptr->tm_min, 10 );
                            s[rc++] = '0' + min.quot;
                            s[rc++] = '0' + min.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'n':
                    {
                        /* newline */
                        s[rc++] = '\n';
                        break;
                    }
                case 'p':
                    {
                        /* tm_hour locale's AM/PM designations */
                        const char * designation = _PDCLIB_lconv.am_pm[ timeptr->tm_hour > 11 ];
                        size_t len = strlen( designation );
                        if ( rc < ( maxsize - len ) )
                        {
                            strcpy( s + rc, designation );
                            rc += len;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'r':
                    {
                        /* tm_hour / tm_min / tm_sec as locale's 12-hour clock time, %I:%M:%S %p for C locale */
                        size_t count = strftime( s + rc, maxsize - rc, _PDCLIB_lconv.time_format_12h, timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'R':
                    {
                        /* %H:%M */
                        size_t count = strftime( s + rc, maxsize - rc, "%H:%M", timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'S':
                    {
                        /* tm_sec as decimal (00-60) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t sec = div( timeptr->tm_sec, 10 );
                            s[rc++] = '0' + sec.quot;
                            s[rc++] = '0' + sec.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 't':
                    {
                        /* tabulator */
                        s[rc++] = '\t';
                        break;
                    }
                case 'T':
                    {
                        /* %H:%M:%S */
                        size_t count = strftime( s + rc, maxsize - rc, "%H:%M:%S", timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'u':
                    {
                        /* tm_wday as decimal (1-7) with Monday == 1 */
                        /* 'O' for locale's alternative numeric symbols */
                        s[rc++] = ( timeptr->tm_wday == 0 ) ? '7' : '0' + timeptr->tm_wday;
                        break;
                    }
                case 'U':
                    {
                        /* week number of the year (first Sunday as the first day of week 1) as decimal (00-53) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t week = div( weeknr( timeptr, E_SUNDAY ), 10 );
                            s[rc++] = '0' + week.quot;
                            s[rc++] = '0' + week.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'V':
                    {
                        /* ISO week number as decimal (01-53) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t week = div( iso_week( timeptr ), 10 );
                            s[rc++] = '0' + week.quot;
                            s[rc++] = '0' + week.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'w':
                    {
                        /* tm_wday as decimal number (0-6) with Sunday == 0 */
                        /* 'O' for locale's alternative numeric symbols */
                        s[rc++] = '0' + timeptr->tm_wday;
                        break;
                    }
                case 'W':
                    {
                        /* week number of the year (first Monday as the first day of week 1) as decimal (00-53) */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t week = div( weeknr( timeptr, E_MONDAY ), 10 );
                            s[rc++] = '0' + week.quot;
                            s[rc++] = '0' + week.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'x':
                    {
                        /* locale's date representation, %m/%d/%y for C locale */
                        /* 'E' for locale's alternative representation */
                        size_t count = strftime( s + rc, maxsize - rc, _PDCLIB_lconv.date_format, timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'X':
                    {
                        /* locale's time representation, %T for C locale */
                        /* 'E' for locale's alternative representation */
                        size_t count = strftime( s + rc, maxsize - rc, _PDCLIB_lconv.time_format, timeptr );
                        if ( count == 0 )
                        {
                            return 0;
                        }
                        else
                        {
                            rc += count;
                        }
                        break;
                    }
                case 'y':
                    {
                        /* last 2 digits of tm_year as decimal (00-99) */
                        /* 'E' for offset from %EC (year only) in locale's alternative representation */
                        /* 'O' for locale's alternative numeric symbols */
                        if ( rc < ( maxsize - 2 ) )
                        {
                            div_t year = div( ( timeptr->tm_year % 100 ), 10 );
                            s[rc++] = '0' + year.quot;
                            s[rc++] = '0' + year.rem;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'Y':
                    {
                        /* tm_year as decimal (e.g. 1997) */
                        /* 'E' for locale's alternative representation */
                        if ( rc < ( maxsize - 4 ) )
                        {
                            int year = timeptr->tm_year + 1900;

                            for ( int i = 3; i >= 0; --i )
                            {
                                div_t digit = div( year, 10 );
                                s[ rc + i ] = '0' + digit.rem;
                                year = digit.quot;
                            }

                            rc += 4;
                        }
                        else
                        {
                            return 0;
                        }
                        break;
                    }
                case 'z':
                    {
                        /* tm_isdst / UTC offset in ISO8601 format (e.g. -0430 meaning 4 hours 30 minutes behind Greenwich), or no characters */
                        /* TODO: 'z' */
                        break;
                    }
                case 'Z':
                    {
                        /* tm_isdst / locale's time zone name or abbreviation, or no characters */
                        /* TODO: 'Z' */
                        break;
                    }
                case '%':
                    {
                        /* '%' character */
                        s[rc++] = '%';
                        break;
                    }
            }
        }
    }

    return 0;
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

#define MKTIME( tm, sec, min, hour, day, month, year, wday, yday ) tm.tm_sec = sec; tm.tm_min = min; tm.tm_hour = hour; tm.tm_mday = day; tm.tm_mon = month; tm.tm_year = year; tm.tm_wday = wday; tm.tm_yday = yday; tm.tm_isdst = -1;

/* Test data generated by reference mktime() / strftime(), listing:
   * tm_year
   * tm_wday
   * tm_yday
   * '%U' result
   * '%V' result
   * '%W' result
*/
int data[1020][6] =
{
{ 70, 4, 0, 0, 1, 0 },
{ 70, 5, 1, 0, 1, 0 },
{ 70, 6, 2, 0, 1, 0 },
{ 70, 0, 3, 1, 1, 0 },
{ 70, 1, 4, 1, 2, 1 },
{ 70, 2, 5, 1, 2, 1 },
{ 70, 3, 6, 1, 2, 1 },
{ 70, 4, 357, 51, 52, 51 },
{ 70, 5, 358, 51, 52, 51 },
{ 70, 6, 359, 51, 52, 51 },
{ 70, 0, 360, 52, 52, 51 },
{ 70, 1, 361, 52, 53, 52 },
{ 70, 2, 362, 52, 53, 52 },
{ 70, 3, 363, 52, 53, 52 },
{ 70, 4, 364, 52, 53, 52 },
{ 71, 5, 0, 0, 53, 0 },
{ 71, 6, 1, 0, 53, 0 },
{ 71, 0, 2, 1, 53, 0 },
{ 71, 1, 3, 1, 1, 1 },
{ 71, 2, 4, 1, 1, 1 },
{ 71, 3, 5, 1, 1, 1 },
{ 71, 4, 6, 1, 1, 1 },
{ 71, 5, 357, 51, 51, 51 },
{ 71, 6, 358, 51, 51, 51 },
{ 71, 0, 359, 52, 51, 51 },
{ 71, 1, 360, 52, 52, 52 },
{ 71, 2, 361, 52, 52, 52 },
{ 71, 3, 362, 52, 52, 52 },
{ 71, 4, 363, 52, 52, 52 },
{ 71, 5, 364, 52, 52, 52 },
{ 72, 6, 0, 0, 52, 0 },
{ 72, 0, 1, 1, 52, 0 },
{ 72, 1, 2, 1, 1, 1 },
{ 72, 2, 3, 1, 1, 1 },
{ 72, 3, 4, 1, 1, 1 },
{ 72, 4, 5, 1, 1, 1 },
{ 72, 5, 6, 1, 1, 1 },
{ 72, 0, 358, 52, 51, 51 },
{ 72, 1, 359, 52, 52, 52 },
{ 72, 2, 360, 52, 52, 52 },
{ 72, 3, 361, 52, 52, 52 },
{ 72, 4, 362, 52, 52, 52 },
{ 72, 5, 363, 52, 52, 52 },
{ 72, 6, 364, 52, 52, 52 },
{ 72, 0, 365, 53, 52, 52 },
{ 73, 1, 0, 0, 1, 1 },
{ 73, 2, 1, 0, 1, 1 },
{ 73, 3, 2, 0, 1, 1 },
{ 73, 4, 3, 0, 1, 1 },
{ 73, 5, 4, 0, 1, 1 },
{ 73, 6, 5, 0, 1, 1 },
{ 73, 0, 6, 1, 1, 1 },
{ 73, 1, 357, 51, 52, 52 },
{ 73, 2, 358, 51, 52, 52 },
{ 73, 3, 359, 51, 52, 52 },
{ 73, 4, 360, 51, 52, 52 },
{ 73, 5, 361, 51, 52, 52 },
{ 73, 6, 362, 51, 52, 52 },
{ 73, 0, 363, 52, 52, 52 },
{ 73, 1, 364, 52, 1, 53 },
{ 74, 2, 0, 0, 1, 0 },
{ 74, 3, 1, 0, 1, 0 },
{ 74, 4, 2, 0, 1, 0 },
{ 74, 5, 3, 0, 1, 0 },
{ 74, 6, 4, 0, 1, 0 },
{ 74, 0, 5, 1, 1, 0 },
{ 74, 1, 6, 1, 2, 1 },
{ 74, 2, 357, 51, 52, 51 },
{ 74, 3, 358, 51, 52, 51 },
{ 74, 4, 359, 51, 52, 51 },
{ 74, 5, 360, 51, 52, 51 },
{ 74, 6, 361, 51, 52, 51 },
{ 74, 0, 362, 52, 52, 51 },
{ 74, 1, 363, 52, 1, 52 },
{ 74, 2, 364, 52, 1, 52 },
{ 75, 3, 0, 0, 1, 0 },
{ 75, 4, 1, 0, 1, 0 },
{ 75, 5, 2, 0, 1, 0 },
{ 75, 6, 3, 0, 1, 0 },
{ 75, 0, 4, 1, 1, 0 },
{ 75, 1, 5, 1, 2, 1 },
{ 75, 2, 6, 1, 2, 1 },
{ 75, 3, 357, 51, 52, 51 },
{ 75, 4, 358, 51, 52, 51 },
{ 75, 5, 359, 51, 52, 51 },
{ 75, 6, 360, 51, 52, 51 },
{ 75, 0, 361, 52, 52, 51 },
{ 75, 1, 362, 52, 1, 52 },
{ 75, 2, 363, 52, 1, 52 },
{ 75, 3, 364, 52, 1, 52 },
{ 76, 4, 0, 0, 1, 0 },
{ 76, 5, 1, 0, 1, 0 },
{ 76, 6, 2, 0, 1, 0 },
{ 76, 0, 3, 1, 1, 0 },
{ 76, 1, 4, 1, 2, 1 },
{ 76, 2, 5, 1, 2, 1 },
{ 76, 3, 6, 1, 2, 1 },
{ 76, 5, 358, 51, 52, 51 },
{ 76, 6, 359, 51, 52, 51 },
{ 76, 0, 360, 52, 52, 51 },
{ 76, 1, 361, 52, 53, 52 },
{ 76, 2, 362, 52, 53, 52 },
{ 76, 3, 363, 52, 53, 52 },
{ 76, 4, 364, 52, 53, 52 },
{ 76, 5, 365, 52, 53, 52 },
{ 77, 6, 0, 0, 53, 0 },
{ 77, 0, 1, 1, 53, 0 },
{ 77, 1, 2, 1, 1, 1 },
{ 77, 2, 3, 1, 1, 1 },
{ 77, 3, 4, 1, 1, 1 },
{ 77, 4, 5, 1, 1, 1 },
{ 77, 5, 6, 1, 1, 1 },
{ 77, 6, 357, 51, 51, 51 },
{ 77, 0, 358, 52, 51, 51 },
{ 77, 1, 359, 52, 52, 52 },
{ 77, 2, 360, 52, 52, 52 },
{ 77, 3, 361, 52, 52, 52 },
{ 77, 4, 362, 52, 52, 52 },
{ 77, 5, 363, 52, 52, 52 },
{ 77, 6, 364, 52, 52, 52 },
{ 78, 0, 0, 1, 52, 0 },
{ 78, 1, 1, 1, 1, 1 },
{ 78, 2, 2, 1, 1, 1 },
{ 78, 3, 3, 1, 1, 1 },
{ 78, 4, 4, 1, 1, 1 },
{ 78, 5, 5, 1, 1, 1 },
{ 78, 6, 6, 1, 1, 1 },
{ 78, 0, 357, 52, 51, 51 },
{ 78, 1, 358, 52, 52, 52 },
{ 78, 2, 359, 52, 52, 52 },
{ 78, 3, 360, 52, 52, 52 },
{ 78, 4, 361, 52, 52, 52 },
{ 78, 5, 362, 52, 52, 52 },
{ 78, 6, 363, 52, 52, 52 },
{ 78, 0, 364, 53, 52, 52 },
{ 79, 1, 0, 0, 1, 1 },
{ 79, 2, 1, 0, 1, 1 },
{ 79, 3, 2, 0, 1, 1 },
{ 79, 4, 3, 0, 1, 1 },
{ 79, 5, 4, 0, 1, 1 },
{ 79, 6, 5, 0, 1, 1 },
{ 79, 0, 6, 1, 1, 1 },
{ 79, 1, 357, 51, 52, 52 },
{ 79, 2, 358, 51, 52, 52 },
{ 79, 3, 359, 51, 52, 52 },
{ 79, 4, 360, 51, 52, 52 },
{ 79, 5, 361, 51, 52, 52 },
{ 79, 6, 362, 51, 52, 52 },
{ 79, 0, 363, 52, 52, 52 },
{ 79, 1, 364, 52, 1, 53 },
{ 80, 2, 0, 0, 1, 0 },
{ 80, 3, 1, 0, 1, 0 },
{ 80, 4, 2, 0, 1, 0 },
{ 80, 5, 3, 0, 1, 0 },
{ 80, 6, 4, 0, 1, 0 },
{ 80, 0, 5, 1, 1, 0 },
{ 80, 1, 6, 1, 2, 1 },
{ 80, 3, 358, 51, 52, 51 },
{ 80, 4, 359, 51, 52, 51 },
{ 80, 5, 360, 51, 52, 51 },
{ 80, 6, 361, 51, 52, 51 },
{ 80, 0, 362, 52, 52, 51 },
{ 80, 1, 363, 52, 1, 52 },
{ 80, 2, 364, 52, 1, 52 },
{ 80, 3, 365, 52, 1, 52 },
{ 81, 4, 0, 0, 1, 0 },
{ 81, 5, 1, 0, 1, 0 },
{ 81, 6, 2, 0, 1, 0 },
{ 81, 0, 3, 1, 1, 0 },
{ 81, 1, 4, 1, 2, 1 },
{ 81, 2, 5, 1, 2, 1 },
{ 81, 3, 6, 1, 2, 1 },
{ 81, 4, 357, 51, 52, 51 },
{ 81, 5, 358, 51, 52, 51 },
{ 81, 6, 359, 51, 52, 51 },
{ 81, 0, 360, 52, 52, 51 },
{ 81, 1, 361, 52, 53, 52 },
{ 81, 2, 362, 52, 53, 52 },
{ 81, 3, 363, 52, 53, 52 },
{ 81, 4, 364, 52, 53, 52 },
{ 82, 5, 0, 0, 53, 0 },
{ 82, 6, 1, 0, 53, 0 },
{ 82, 0, 2, 1, 53, 0 },
{ 82, 1, 3, 1, 1, 1 },
{ 82, 2, 4, 1, 1, 1 },
{ 82, 3, 5, 1, 1, 1 },
{ 82, 4, 6, 1, 1, 1 },
{ 82, 5, 357, 51, 51, 51 },
{ 82, 6, 358, 51, 51, 51 },
{ 82, 0, 359, 52, 51, 51 },
{ 82, 1, 360, 52, 52, 52 },
{ 82, 2, 361, 52, 52, 52 },
{ 82, 3, 362, 52, 52, 52 },
{ 82, 4, 363, 52, 52, 52 },
{ 82, 5, 364, 52, 52, 52 },
{ 83, 6, 0, 0, 52, 0 },
{ 83, 0, 1, 1, 52, 0 },
{ 83, 1, 2, 1, 1, 1 },
{ 83, 2, 3, 1, 1, 1 },
{ 83, 3, 4, 1, 1, 1 },
{ 83, 4, 5, 1, 1, 1 },
{ 83, 5, 6, 1, 1, 1 },
{ 83, 6, 357, 51, 51, 51 },
{ 83, 0, 358, 52, 51, 51 },
{ 83, 1, 359, 52, 52, 52 },
{ 83, 2, 360, 52, 52, 52 },
{ 83, 3, 361, 52, 52, 52 },
{ 83, 4, 362, 52, 52, 52 },
{ 83, 5, 363, 52, 52, 52 },
{ 83, 6, 364, 52, 52, 52 },
{ 84, 0, 0, 1, 52, 0 },
{ 84, 1, 1, 1, 1, 1 },
{ 84, 2, 2, 1, 1, 1 },
{ 84, 3, 3, 1, 1, 1 },
{ 84, 4, 4, 1, 1, 1 },
{ 84, 5, 5, 1, 1, 1 },
{ 84, 6, 6, 1, 1, 1 },
{ 84, 1, 358, 52, 52, 52 },
{ 84, 2, 359, 52, 52, 52 },
{ 84, 3, 360, 52, 52, 52 },
{ 84, 4, 361, 52, 52, 52 },
{ 84, 5, 362, 52, 52, 52 },
{ 84, 6, 363, 52, 52, 52 },
{ 84, 0, 364, 53, 52, 52 },
{ 84, 1, 365, 53, 1, 53 },
{ 85, 2, 0, 0, 1, 0 },
{ 85, 3, 1, 0, 1, 0 },
{ 85, 4, 2, 0, 1, 0 },
{ 85, 5, 3, 0, 1, 0 },
{ 85, 6, 4, 0, 1, 0 },
{ 85, 0, 5, 1, 1, 0 },
{ 85, 1, 6, 1, 2, 1 },
{ 85, 2, 357, 51, 52, 51 },
{ 85, 3, 358, 51, 52, 51 },
{ 85, 4, 359, 51, 52, 51 },
{ 85, 5, 360, 51, 52, 51 },
{ 85, 6, 361, 51, 52, 51 },
{ 85, 0, 362, 52, 52, 51 },
{ 85, 1, 363, 52, 1, 52 },
{ 85, 2, 364, 52, 1, 52 },
{ 86, 3, 0, 0, 1, 0 },
{ 86, 4, 1, 0, 1, 0 },
{ 86, 5, 2, 0, 1, 0 },
{ 86, 6, 3, 0, 1, 0 },
{ 86, 0, 4, 1, 1, 0 },
{ 86, 1, 5, 1, 2, 1 },
{ 86, 2, 6, 1, 2, 1 },
{ 86, 3, 357, 51, 52, 51 },
{ 86, 4, 358, 51, 52, 51 },
{ 86, 5, 359, 51, 52, 51 },
{ 86, 6, 360, 51, 52, 51 },
{ 86, 0, 361, 52, 52, 51 },
{ 86, 1, 362, 52, 1, 52 },
{ 86, 2, 363, 52, 1, 52 },
{ 86, 3, 364, 52, 1, 52 },
{ 87, 4, 0, 0, 1, 0 },
{ 87, 5, 1, 0, 1, 0 },
{ 87, 6, 2, 0, 1, 0 },
{ 87, 0, 3, 1, 1, 0 },
{ 87, 1, 4, 1, 2, 1 },
{ 87, 2, 5, 1, 2, 1 },
{ 87, 3, 6, 1, 2, 1 },
{ 87, 4, 357, 51, 52, 51 },
{ 87, 5, 358, 51, 52, 51 },
{ 87, 6, 359, 51, 52, 51 },
{ 87, 0, 360, 52, 52, 51 },
{ 87, 1, 361, 52, 53, 52 },
{ 87, 2, 362, 52, 53, 52 },
{ 87, 3, 363, 52, 53, 52 },
{ 87, 4, 364, 52, 53, 52 },
{ 88, 5, 0, 0, 53, 0 },
{ 88, 6, 1, 0, 53, 0 },
{ 88, 0, 2, 1, 53, 0 },
{ 88, 1, 3, 1, 1, 1 },
{ 88, 2, 4, 1, 1, 1 },
{ 88, 3, 5, 1, 1, 1 },
{ 88, 4, 6, 1, 1, 1 },
{ 88, 6, 358, 51, 51, 51 },
{ 88, 0, 359, 52, 51, 51 },
{ 88, 1, 360, 52, 52, 52 },
{ 88, 2, 361, 52, 52, 52 },
{ 88, 3, 362, 52, 52, 52 },
{ 88, 4, 363, 52, 52, 52 },
{ 88, 5, 364, 52, 52, 52 },
{ 88, 6, 365, 52, 52, 52 },
{ 89, 0, 0, 1, 52, 0 },
{ 89, 1, 1, 1, 1, 1 },
{ 89, 2, 2, 1, 1, 1 },
{ 89, 3, 3, 1, 1, 1 },
{ 89, 4, 4, 1, 1, 1 },
{ 89, 5, 5, 1, 1, 1 },
{ 89, 6, 6, 1, 1, 1 },
{ 89, 0, 357, 52, 51, 51 },
{ 89, 1, 358, 52, 52, 52 },
{ 89, 2, 359, 52, 52, 52 },
{ 89, 3, 360, 52, 52, 52 },
{ 89, 4, 361, 52, 52, 52 },
{ 89, 5, 362, 52, 52, 52 },
{ 89, 6, 363, 52, 52, 52 },
{ 89, 0, 364, 53, 52, 52 },
{ 90, 1, 0, 0, 1, 1 },
{ 90, 2, 1, 0, 1, 1 },
{ 90, 3, 2, 0, 1, 1 },
{ 90, 4, 3, 0, 1, 1 },
{ 90, 5, 4, 0, 1, 1 },
{ 90, 6, 5, 0, 1, 1 },
{ 90, 0, 6, 1, 1, 1 },
{ 90, 1, 357, 51, 52, 52 },
{ 90, 2, 358, 51, 52, 52 },
{ 90, 3, 359, 51, 52, 52 },
{ 90, 4, 360, 51, 52, 52 },
{ 90, 5, 361, 51, 52, 52 },
{ 90, 6, 362, 51, 52, 52 },
{ 90, 0, 363, 52, 52, 52 },
{ 90, 1, 364, 52, 1, 53 },
{ 91, 2, 0, 0, 1, 0 },
{ 91, 3, 1, 0, 1, 0 },
{ 91, 4, 2, 0, 1, 0 },
{ 91, 5, 3, 0, 1, 0 },
{ 91, 6, 4, 0, 1, 0 },
{ 91, 0, 5, 1, 1, 0 },
{ 91, 1, 6, 1, 2, 1 },
{ 91, 2, 357, 51, 52, 51 },
{ 91, 3, 358, 51, 52, 51 },
{ 91, 4, 359, 51, 52, 51 },
{ 91, 5, 360, 51, 52, 51 },
{ 91, 6, 361, 51, 52, 51 },
{ 91, 0, 362, 52, 52, 51 },
{ 91, 1, 363, 52, 1, 52 },
{ 91, 2, 364, 52, 1, 52 },
{ 92, 3, 0, 0, 1, 0 },
{ 92, 4, 1, 0, 1, 0 },
{ 92, 5, 2, 0, 1, 0 },
{ 92, 6, 3, 0, 1, 0 },
{ 92, 0, 4, 1, 1, 0 },
{ 92, 1, 5, 1, 2, 1 },
{ 92, 2, 6, 1, 2, 1 },
{ 92, 4, 358, 51, 52, 51 },
{ 92, 5, 359, 51, 52, 51 },
{ 92, 6, 360, 51, 52, 51 },
{ 92, 0, 361, 52, 52, 51 },
{ 92, 1, 362, 52, 53, 52 },
{ 92, 2, 363, 52, 53, 52 },
{ 92, 3, 364, 52, 53, 52 },
{ 92, 4, 365, 52, 53, 52 },
{ 93, 5, 0, 0, 53, 0 },
{ 93, 6, 1, 0, 53, 0 },
{ 93, 0, 2, 1, 53, 0 },
{ 93, 1, 3, 1, 1, 1 },
{ 93, 2, 4, 1, 1, 1 },
{ 93, 3, 5, 1, 1, 1 },
{ 93, 4, 6, 1, 1, 1 },
{ 93, 5, 357, 51, 51, 51 },
{ 93, 6, 358, 51, 51, 51 },
{ 93, 0, 359, 52, 51, 51 },
{ 93, 1, 360, 52, 52, 52 },
{ 93, 2, 361, 52, 52, 52 },
{ 93, 3, 362, 52, 52, 52 },
{ 93, 4, 363, 52, 52, 52 },
{ 93, 5, 364, 52, 52, 52 },
{ 94, 6, 0, 0, 52, 0 },
{ 94, 0, 1, 1, 52, 0 },
{ 94, 1, 2, 1, 1, 1 },
{ 94, 2, 3, 1, 1, 1 },
{ 94, 3, 4, 1, 1, 1 },
{ 94, 4, 5, 1, 1, 1 },
{ 94, 5, 6, 1, 1, 1 },
{ 94, 6, 357, 51, 51, 51 },
{ 94, 0, 358, 52, 51, 51 },
{ 94, 1, 359, 52, 52, 52 },
{ 94, 2, 360, 52, 52, 52 },
{ 94, 3, 361, 52, 52, 52 },
{ 94, 4, 362, 52, 52, 52 },
{ 94, 5, 363, 52, 52, 52 },
{ 94, 6, 364, 52, 52, 52 },
{ 95, 0, 0, 1, 52, 0 },
{ 95, 1, 1, 1, 1, 1 },
{ 95, 2, 2, 1, 1, 1 },
{ 95, 3, 3, 1, 1, 1 },
{ 95, 4, 4, 1, 1, 1 },
{ 95, 5, 5, 1, 1, 1 },
{ 95, 6, 6, 1, 1, 1 },
{ 95, 0, 357, 52, 51, 51 },
{ 95, 1, 358, 52, 52, 52 },
{ 95, 2, 359, 52, 52, 52 },
{ 95, 3, 360, 52, 52, 52 },
{ 95, 4, 361, 52, 52, 52 },
{ 95, 5, 362, 52, 52, 52 },
{ 95, 6, 363, 52, 52, 52 },
{ 95, 0, 364, 53, 52, 52 },
{ 96, 1, 0, 0, 1, 1 },
{ 96, 2, 1, 0, 1, 1 },
{ 96, 3, 2, 0, 1, 1 },
{ 96, 4, 3, 0, 1, 1 },
{ 96, 5, 4, 0, 1, 1 },
{ 96, 6, 5, 0, 1, 1 },
{ 96, 0, 6, 1, 1, 1 },
{ 96, 2, 358, 51, 52, 52 },
{ 96, 3, 359, 51, 52, 52 },
{ 96, 4, 360, 51, 52, 52 },
{ 96, 5, 361, 51, 52, 52 },
{ 96, 6, 362, 51, 52, 52 },
{ 96, 0, 363, 52, 52, 52 },
{ 96, 1, 364, 52, 1, 53 },
{ 96, 2, 365, 52, 1, 53 },
{ 97, 3, 0, 0, 1, 0 },
{ 97, 4, 1, 0, 1, 0 },
{ 97, 5, 2, 0, 1, 0 },
{ 97, 6, 3, 0, 1, 0 },
{ 97, 0, 4, 1, 1, 0 },
{ 97, 1, 5, 1, 2, 1 },
{ 97, 2, 6, 1, 2, 1 },
{ 97, 3, 357, 51, 52, 51 },
{ 97, 4, 358, 51, 52, 51 },
{ 97, 5, 359, 51, 52, 51 },
{ 97, 6, 360, 51, 52, 51 },
{ 97, 0, 361, 52, 52, 51 },
{ 97, 1, 362, 52, 1, 52 },
{ 97, 2, 363, 52, 1, 52 },
{ 97, 3, 364, 52, 1, 52 },
{ 98, 4, 0, 0, 1, 0 },
{ 98, 5, 1, 0, 1, 0 },
{ 98, 6, 2, 0, 1, 0 },
{ 98, 0, 3, 1, 1, 0 },
{ 98, 1, 4, 1, 2, 1 },
{ 98, 2, 5, 1, 2, 1 },
{ 98, 3, 6, 1, 2, 1 },
{ 98, 4, 357, 51, 52, 51 },
{ 98, 5, 358, 51, 52, 51 },
{ 98, 6, 359, 51, 52, 51 },
{ 98, 0, 360, 52, 52, 51 },
{ 98, 1, 361, 52, 53, 52 },
{ 98, 2, 362, 52, 53, 52 },
{ 98, 3, 363, 52, 53, 52 },
{ 98, 4, 364, 52, 53, 52 },
{ 99, 5, 0, 0, 53, 0 },
{ 99, 6, 1, 0, 53, 0 },
{ 99, 0, 2, 1, 53, 0 },
{ 99, 1, 3, 1, 1, 1 },
{ 99, 2, 4, 1, 1, 1 },
{ 99, 3, 5, 1, 1, 1 },
{ 99, 4, 6, 1, 1, 1 },
{ 99, 5, 357, 51, 51, 51 },
{ 99, 6, 358, 51, 51, 51 },
{ 99, 0, 359, 52, 51, 51 },
{ 99, 1, 360, 52, 52, 52 },
{ 99, 2, 361, 52, 52, 52 },
{ 99, 3, 362, 52, 52, 52 },
{ 99, 4, 363, 52, 52, 52 },
{ 99, 5, 364, 52, 52, 52 },
{ 100, 6, 0, 0, 52, 0 },
{ 100, 0, 1, 1, 52, 0 },
{ 100, 1, 2, 1, 1, 1 },
{ 100, 2, 3, 1, 1, 1 },
{ 100, 3, 4, 1, 1, 1 },
{ 100, 4, 5, 1, 1, 1 },
{ 100, 5, 6, 1, 1, 1 },
{ 100, 0, 358, 52, 51, 51 },
{ 100, 1, 359, 52, 52, 52 },
{ 100, 2, 360, 52, 52, 52 },
{ 100, 3, 361, 52, 52, 52 },
{ 100, 4, 362, 52, 52, 52 },
{ 100, 5, 363, 52, 52, 52 },
{ 100, 6, 364, 52, 52, 52 },
{ 100, 0, 365, 53, 52, 52 },
{ 101, 1, 0, 0, 1, 1 },
{ 101, 2, 1, 0, 1, 1 },
{ 101, 3, 2, 0, 1, 1 },
{ 101, 4, 3, 0, 1, 1 },
{ 101, 5, 4, 0, 1, 1 },
{ 101, 6, 5, 0, 1, 1 },
{ 101, 0, 6, 1, 1, 1 },
{ 101, 1, 357, 51, 52, 52 },
{ 101, 2, 358, 51, 52, 52 },
{ 101, 3, 359, 51, 52, 52 },
{ 101, 4, 360, 51, 52, 52 },
{ 101, 5, 361, 51, 52, 52 },
{ 101, 6, 362, 51, 52, 52 },
{ 101, 0, 363, 52, 52, 52 },
{ 101, 1, 364, 52, 1, 53 },
{ 102, 2, 0, 0, 1, 0 },
{ 102, 3, 1, 0, 1, 0 },
{ 102, 4, 2, 0, 1, 0 },
{ 102, 5, 3, 0, 1, 0 },
{ 102, 6, 4, 0, 1, 0 },
{ 102, 0, 5, 1, 1, 0 },
{ 102, 1, 6, 1, 2, 1 },
{ 102, 2, 357, 51, 52, 51 },
{ 102, 3, 358, 51, 52, 51 },
{ 102, 4, 359, 51, 52, 51 },
{ 102, 5, 360, 51, 52, 51 },
{ 102, 6, 361, 51, 52, 51 },
{ 102, 0, 362, 52, 52, 51 },
{ 102, 1, 363, 52, 1, 52 },
{ 102, 2, 364, 52, 1, 52 },
{ 103, 3, 0, 0, 1, 0 },
{ 103, 4, 1, 0, 1, 0 },
{ 103, 5, 2, 0, 1, 0 },
{ 103, 6, 3, 0, 1, 0 },
{ 103, 0, 4, 1, 1, 0 },
{ 103, 1, 5, 1, 2, 1 },
{ 103, 2, 6, 1, 2, 1 },
{ 103, 3, 357, 51, 52, 51 },
{ 103, 4, 358, 51, 52, 51 },
{ 103, 5, 359, 51, 52, 51 },
{ 103, 6, 360, 51, 52, 51 },
{ 103, 0, 361, 52, 52, 51 },
{ 103, 1, 362, 52, 1, 52 },
{ 103, 2, 363, 52, 1, 52 },
{ 103, 3, 364, 52, 1, 52 },
{ 104, 4, 0, 0, 1, 0 },
{ 104, 5, 1, 0, 1, 0 },
{ 104, 6, 2, 0, 1, 0 },
{ 104, 0, 3, 1, 1, 0 },
{ 104, 1, 4, 1, 2, 1 },
{ 104, 2, 5, 1, 2, 1 },
{ 104, 3, 6, 1, 2, 1 },
{ 104, 5, 358, 51, 52, 51 },
{ 104, 6, 359, 51, 52, 51 },
{ 104, 0, 360, 52, 52, 51 },
{ 104, 1, 361, 52, 53, 52 },
{ 104, 2, 362, 52, 53, 52 },
{ 104, 3, 363, 52, 53, 52 },
{ 104, 4, 364, 52, 53, 52 },
{ 104, 5, 365, 52, 53, 52 },
{ 105, 6, 0, 0, 53, 0 },
{ 105, 0, 1, 1, 53, 0 },
{ 105, 1, 2, 1, 1, 1 },
{ 105, 2, 3, 1, 1, 1 },
{ 105, 3, 4, 1, 1, 1 },
{ 105, 4, 5, 1, 1, 1 },
{ 105, 5, 6, 1, 1, 1 },
{ 105, 6, 357, 51, 51, 51 },
{ 105, 0, 358, 52, 51, 51 },
{ 105, 1, 359, 52, 52, 52 },
{ 105, 2, 360, 52, 52, 52 },
{ 105, 3, 361, 52, 52, 52 },
{ 105, 4, 362, 52, 52, 52 },
{ 105, 5, 363, 52, 52, 52 },
{ 105, 6, 364, 52, 52, 52 },
{ 106, 0, 0, 1, 52, 0 },
{ 106, 1, 1, 1, 1, 1 },
{ 106, 2, 2, 1, 1, 1 },
{ 106, 3, 3, 1, 1, 1 },
{ 106, 4, 4, 1, 1, 1 },
{ 106, 5, 5, 1, 1, 1 },
{ 106, 6, 6, 1, 1, 1 },
{ 106, 0, 357, 52, 51, 51 },
{ 106, 1, 358, 52, 52, 52 },
{ 106, 2, 359, 52, 52, 52 },
{ 106, 3, 360, 52, 52, 52 },
{ 106, 4, 361, 52, 52, 52 },
{ 106, 5, 362, 52, 52, 52 },
{ 106, 6, 363, 52, 52, 52 },
{ 106, 0, 364, 53, 52, 52 },
{ 107, 1, 0, 0, 1, 1 },
{ 107, 2, 1, 0, 1, 1 },
{ 107, 3, 2, 0, 1, 1 },
{ 107, 4, 3, 0, 1, 1 },
{ 107, 5, 4, 0, 1, 1 },
{ 107, 6, 5, 0, 1, 1 },
{ 107, 0, 6, 1, 1, 1 },
{ 107, 1, 357, 51, 52, 52 },
{ 107, 2, 358, 51, 52, 52 },
{ 107, 3, 359, 51, 52, 52 },
{ 107, 4, 360, 51, 52, 52 },
{ 107, 5, 361, 51, 52, 52 },
{ 107, 6, 362, 51, 52, 52 },
{ 107, 0, 363, 52, 52, 52 },
{ 107, 1, 364, 52, 1, 53 },
{ 108, 2, 0, 0, 1, 0 },
{ 108, 3, 1, 0, 1, 0 },
{ 108, 4, 2, 0, 1, 0 },
{ 108, 5, 3, 0, 1, 0 },
{ 108, 6, 4, 0, 1, 0 },
{ 108, 0, 5, 1, 1, 0 },
{ 108, 1, 6, 1, 2, 1 },
{ 108, 3, 358, 51, 52, 51 },
{ 108, 4, 359, 51, 52, 51 },
{ 108, 5, 360, 51, 52, 51 },
{ 108, 6, 361, 51, 52, 51 },
{ 108, 0, 362, 52, 52, 51 },
{ 108, 1, 363, 52, 1, 52 },
{ 108, 2, 364, 52, 1, 52 },
{ 108, 3, 365, 52, 1, 52 },
{ 109, 4, 0, 0, 1, 0 },
{ 109, 5, 1, 0, 1, 0 },
{ 109, 6, 2, 0, 1, 0 },
{ 109, 0, 3, 1, 1, 0 },
{ 109, 1, 4, 1, 2, 1 },
{ 109, 2, 5, 1, 2, 1 },
{ 109, 3, 6, 1, 2, 1 },
{ 109, 4, 357, 51, 52, 51 },
{ 109, 5, 358, 51, 52, 51 },
{ 109, 6, 359, 51, 52, 51 },
{ 109, 0, 360, 52, 52, 51 },
{ 109, 1, 361, 52, 53, 52 },
{ 109, 2, 362, 52, 53, 52 },
{ 109, 3, 363, 52, 53, 52 },
{ 109, 4, 364, 52, 53, 52 },
{ 110, 5, 0, 0, 53, 0 },
{ 110, 6, 1, 0, 53, 0 },
{ 110, 0, 2, 1, 53, 0 },
{ 110, 1, 3, 1, 1, 1 },
{ 110, 2, 4, 1, 1, 1 },
{ 110, 3, 5, 1, 1, 1 },
{ 110, 4, 6, 1, 1, 1 },
{ 110, 5, 357, 51, 51, 51 },
{ 110, 6, 358, 51, 51, 51 },
{ 110, 0, 359, 52, 51, 51 },
{ 110, 1, 360, 52, 52, 52 },
{ 110, 2, 361, 52, 52, 52 },
{ 110, 3, 362, 52, 52, 52 },
{ 110, 4, 363, 52, 52, 52 },
{ 110, 5, 364, 52, 52, 52 },
{ 111, 6, 0, 0, 52, 0 },
{ 111, 0, 1, 1, 52, 0 },
{ 111, 1, 2, 1, 1, 1 },
{ 111, 2, 3, 1, 1, 1 },
{ 111, 3, 4, 1, 1, 1 },
{ 111, 4, 5, 1, 1, 1 },
{ 111, 5, 6, 1, 1, 1 },
{ 111, 6, 357, 51, 51, 51 },
{ 111, 0, 358, 52, 51, 51 },
{ 111, 1, 359, 52, 52, 52 },
{ 111, 2, 360, 52, 52, 52 },
{ 111, 3, 361, 52, 52, 52 },
{ 111, 4, 362, 52, 52, 52 },
{ 111, 5, 363, 52, 52, 52 },
{ 111, 6, 364, 52, 52, 52 },
{ 112, 0, 0, 1, 52, 0 },
{ 112, 1, 1, 1, 1, 1 },
{ 112, 2, 2, 1, 1, 1 },
{ 112, 3, 3, 1, 1, 1 },
{ 112, 4, 4, 1, 1, 1 },
{ 112, 5, 5, 1, 1, 1 },
{ 112, 6, 6, 1, 1, 1 },
{ 112, 1, 358, 52, 52, 52 },
{ 112, 2, 359, 52, 52, 52 },
{ 112, 3, 360, 52, 52, 52 },
{ 112, 4, 361, 52, 52, 52 },
{ 112, 5, 362, 52, 52, 52 },
{ 112, 6, 363, 52, 52, 52 },
{ 112, 0, 364, 53, 52, 52 },
{ 112, 1, 365, 53, 1, 53 },
{ 113, 2, 0, 0, 1, 0 },
{ 113, 3, 1, 0, 1, 0 },
{ 113, 4, 2, 0, 1, 0 },
{ 113, 5, 3, 0, 1, 0 },
{ 113, 6, 4, 0, 1, 0 },
{ 113, 0, 5, 1, 1, 0 },
{ 113, 1, 6, 1, 2, 1 },
{ 113, 2, 357, 51, 52, 51 },
{ 113, 3, 358, 51, 52, 51 },
{ 113, 4, 359, 51, 52, 51 },
{ 113, 5, 360, 51, 52, 51 },
{ 113, 6, 361, 51, 52, 51 },
{ 113, 0, 362, 52, 52, 51 },
{ 113, 1, 363, 52, 1, 52 },
{ 113, 2, 364, 52, 1, 52 },
{ 114, 3, 0, 0, 1, 0 },
{ 114, 4, 1, 0, 1, 0 },
{ 114, 5, 2, 0, 1, 0 },
{ 114, 6, 3, 0, 1, 0 },
{ 114, 0, 4, 1, 1, 0 },
{ 114, 1, 5, 1, 2, 1 },
{ 114, 2, 6, 1, 2, 1 },
{ 114, 3, 357, 51, 52, 51 },
{ 114, 4, 358, 51, 52, 51 },
{ 114, 5, 359, 51, 52, 51 },
{ 114, 6, 360, 51, 52, 51 },
{ 114, 0, 361, 52, 52, 51 },
{ 114, 1, 362, 52, 1, 52 },
{ 114, 2, 363, 52, 1, 52 },
{ 114, 3, 364, 52, 1, 52 },
{ 115, 4, 0, 0, 1, 0 },
{ 115, 5, 1, 0, 1, 0 },
{ 115, 6, 2, 0, 1, 0 },
{ 115, 0, 3, 1, 1, 0 },
{ 115, 1, 4, 1, 2, 1 },
{ 115, 2, 5, 1, 2, 1 },
{ 115, 3, 6, 1, 2, 1 },
{ 115, 4, 357, 51, 52, 51 },
{ 115, 5, 358, 51, 52, 51 },
{ 115, 6, 359, 51, 52, 51 },
{ 115, 0, 360, 52, 52, 51 },
{ 115, 1, 361, 52, 53, 52 },
{ 115, 2, 362, 52, 53, 52 },
{ 115, 3, 363, 52, 53, 52 },
{ 115, 4, 364, 52, 53, 52 },
{ 116, 5, 0, 0, 53, 0 },
{ 116, 6, 1, 0, 53, 0 },
{ 116, 0, 2, 1, 53, 0 },
{ 116, 1, 3, 1, 1, 1 },
{ 116, 2, 4, 1, 1, 1 },
{ 116, 3, 5, 1, 1, 1 },
{ 116, 4, 6, 1, 1, 1 },
{ 116, 6, 358, 51, 51, 51 },
{ 116, 0, 359, 52, 51, 51 },
{ 116, 1, 360, 52, 52, 52 },
{ 116, 2, 361, 52, 52, 52 },
{ 116, 3, 362, 52, 52, 52 },
{ 116, 4, 363, 52, 52, 52 },
{ 116, 5, 364, 52, 52, 52 },
{ 116, 6, 365, 52, 52, 52 },
{ 117, 0, 0, 1, 52, 0 },
{ 117, 1, 1, 1, 1, 1 },
{ 117, 2, 2, 1, 1, 1 },
{ 117, 3, 3, 1, 1, 1 },
{ 117, 4, 4, 1, 1, 1 },
{ 117, 5, 5, 1, 1, 1 },
{ 117, 6, 6, 1, 1, 1 },
{ 117, 0, 357, 52, 51, 51 },
{ 117, 1, 358, 52, 52, 52 },
{ 117, 2, 359, 52, 52, 52 },
{ 117, 3, 360, 52, 52, 52 },
{ 117, 4, 361, 52, 52, 52 },
{ 117, 5, 362, 52, 52, 52 },
{ 117, 6, 363, 52, 52, 52 },
{ 117, 0, 364, 53, 52, 52 },
{ 118, 1, 0, 0, 1, 1 },
{ 118, 2, 1, 0, 1, 1 },
{ 118, 3, 2, 0, 1, 1 },
{ 118, 4, 3, 0, 1, 1 },
{ 118, 5, 4, 0, 1, 1 },
{ 118, 6, 5, 0, 1, 1 },
{ 118, 0, 6, 1, 1, 1 },
{ 118, 1, 357, 51, 52, 52 },
{ 118, 2, 358, 51, 52, 52 },
{ 118, 3, 359, 51, 52, 52 },
{ 118, 4, 360, 51, 52, 52 },
{ 118, 5, 361, 51, 52, 52 },
{ 118, 6, 362, 51, 52, 52 },
{ 118, 0, 363, 52, 52, 52 },
{ 118, 1, 364, 52, 1, 53 },
{ 119, 2, 0, 0, 1, 0 },
{ 119, 3, 1, 0, 1, 0 },
{ 119, 4, 2, 0, 1, 0 },
{ 119, 5, 3, 0, 1, 0 },
{ 119, 6, 4, 0, 1, 0 },
{ 119, 0, 5, 1, 1, 0 },
{ 119, 1, 6, 1, 2, 1 },
{ 119, 2, 357, 51, 52, 51 },
{ 119, 3, 358, 51, 52, 51 },
{ 119, 4, 359, 51, 52, 51 },
{ 119, 5, 360, 51, 52, 51 },
{ 119, 6, 361, 51, 52, 51 },
{ 119, 0, 362, 52, 52, 51 },
{ 119, 1, 363, 52, 1, 52 },
{ 119, 2, 364, 52, 1, 52 },
{ 120, 3, 0, 0, 1, 0 },
{ 120, 4, 1, 0, 1, 0 },
{ 120, 5, 2, 0, 1, 0 },
{ 120, 6, 3, 0, 1, 0 },
{ 120, 0, 4, 1, 1, 0 },
{ 120, 1, 5, 1, 2, 1 },
{ 120, 2, 6, 1, 2, 1 },
{ 120, 4, 358, 51, 52, 51 },
{ 120, 5, 359, 51, 52, 51 },
{ 120, 6, 360, 51, 52, 51 },
{ 120, 0, 361, 52, 52, 51 },
{ 120, 1, 362, 52, 53, 52 },
{ 120, 2, 363, 52, 53, 52 },
{ 120, 3, 364, 52, 53, 52 },
{ 120, 4, 365, 52, 53, 52 },
{ 121, 5, 0, 0, 53, 0 },
{ 121, 6, 1, 0, 53, 0 },
{ 121, 0, 2, 1, 53, 0 },
{ 121, 1, 3, 1, 1, 1 },
{ 121, 2, 4, 1, 1, 1 },
{ 121, 3, 5, 1, 1, 1 },
{ 121, 4, 6, 1, 1, 1 },
{ 121, 5, 357, 51, 51, 51 },
{ 121, 6, 358, 51, 51, 51 },
{ 121, 0, 359, 52, 51, 51 },
{ 121, 1, 360, 52, 52, 52 },
{ 121, 2, 361, 52, 52, 52 },
{ 121, 3, 362, 52, 52, 52 },
{ 121, 4, 363, 52, 52, 52 },
{ 121, 5, 364, 52, 52, 52 },
{ 122, 6, 0, 0, 52, 0 },
{ 122, 0, 1, 1, 52, 0 },
{ 122, 1, 2, 1, 1, 1 },
{ 122, 2, 3, 1, 1, 1 },
{ 122, 3, 4, 1, 1, 1 },
{ 122, 4, 5, 1, 1, 1 },
{ 122, 5, 6, 1, 1, 1 },
{ 122, 6, 357, 51, 51, 51 },
{ 122, 0, 358, 52, 51, 51 },
{ 122, 1, 359, 52, 52, 52 },
{ 122, 2, 360, 52, 52, 52 },
{ 122, 3, 361, 52, 52, 52 },
{ 122, 4, 362, 52, 52, 52 },
{ 122, 5, 363, 52, 52, 52 },
{ 122, 6, 364, 52, 52, 52 },
{ 123, 0, 0, 1, 52, 0 },
{ 123, 1, 1, 1, 1, 1 },
{ 123, 2, 2, 1, 1, 1 },
{ 123, 3, 3, 1, 1, 1 },
{ 123, 4, 4, 1, 1, 1 },
{ 123, 5, 5, 1, 1, 1 },
{ 123, 6, 6, 1, 1, 1 },
{ 123, 0, 357, 52, 51, 51 },
{ 123, 1, 358, 52, 52, 52 },
{ 123, 2, 359, 52, 52, 52 },
{ 123, 3, 360, 52, 52, 52 },
{ 123, 4, 361, 52, 52, 52 },
{ 123, 5, 362, 52, 52, 52 },
{ 123, 6, 363, 52, 52, 52 },
{ 123, 0, 364, 53, 52, 52 },
{ 124, 1, 0, 0, 1, 1 },
{ 124, 2, 1, 0, 1, 1 },
{ 124, 3, 2, 0, 1, 1 },
{ 124, 4, 3, 0, 1, 1 },
{ 124, 5, 4, 0, 1, 1 },
{ 124, 6, 5, 0, 1, 1 },
{ 124, 0, 6, 1, 1, 1 },
{ 124, 2, 358, 51, 52, 52 },
{ 124, 3, 359, 51, 52, 52 },
{ 124, 4, 360, 51, 52, 52 },
{ 124, 5, 361, 51, 52, 52 },
{ 124, 6, 362, 51, 52, 52 },
{ 124, 0, 363, 52, 52, 52 },
{ 124, 1, 364, 52, 1, 53 },
{ 124, 2, 365, 52, 1, 53 },
{ 125, 3, 0, 0, 1, 0 },
{ 125, 4, 1, 0, 1, 0 },
{ 125, 5, 2, 0, 1, 0 },
{ 125, 6, 3, 0, 1, 0 },
{ 125, 0, 4, 1, 1, 0 },
{ 125, 1, 5, 1, 2, 1 },
{ 125, 2, 6, 1, 2, 1 },
{ 125, 3, 357, 51, 52, 51 },
{ 125, 4, 358, 51, 52, 51 },
{ 125, 5, 359, 51, 52, 51 },
{ 125, 6, 360, 51, 52, 51 },
{ 125, 0, 361, 52, 52, 51 },
{ 125, 1, 362, 52, 1, 52 },
{ 125, 2, 363, 52, 1, 52 },
{ 125, 3, 364, 52, 1, 52 },
{ 126, 4, 0, 0, 1, 0 },
{ 126, 5, 1, 0, 1, 0 },
{ 126, 6, 2, 0, 1, 0 },
{ 126, 0, 3, 1, 1, 0 },
{ 126, 1, 4, 1, 2, 1 },
{ 126, 2, 5, 1, 2, 1 },
{ 126, 3, 6, 1, 2, 1 },
{ 126, 4, 357, 51, 52, 51 },
{ 126, 5, 358, 51, 52, 51 },
{ 126, 6, 359, 51, 52, 51 },
{ 126, 0, 360, 52, 52, 51 },
{ 126, 1, 361, 52, 53, 52 },
{ 126, 2, 362, 52, 53, 52 },
{ 126, 3, 363, 52, 53, 52 },
{ 126, 4, 364, 52, 53, 52 },
{ 127, 5, 0, 0, 53, 0 },
{ 127, 6, 1, 0, 53, 0 },
{ 127, 0, 2, 1, 53, 0 },
{ 127, 1, 3, 1, 1, 1 },
{ 127, 2, 4, 1, 1, 1 },
{ 127, 3, 5, 1, 1, 1 },
{ 127, 4, 6, 1, 1, 1 },
{ 127, 5, 357, 51, 51, 51 },
{ 127, 6, 358, 51, 51, 51 },
{ 127, 0, 359, 52, 51, 51 },
{ 127, 1, 360, 52, 52, 52 },
{ 127, 2, 361, 52, 52, 52 },
{ 127, 3, 362, 52, 52, 52 },
{ 127, 4, 363, 52, 52, 52 },
{ 127, 5, 364, 52, 52, 52 },
{ 128, 6, 0, 0, 52, 0 },
{ 128, 0, 1, 1, 52, 0 },
{ 128, 1, 2, 1, 1, 1 },
{ 128, 2, 3, 1, 1, 1 },
{ 128, 3, 4, 1, 1, 1 },
{ 128, 4, 5, 1, 1, 1 },
{ 128, 5, 6, 1, 1, 1 },
{ 128, 0, 358, 52, 51, 51 },
{ 128, 1, 359, 52, 52, 52 },
{ 128, 2, 360, 52, 52, 52 },
{ 128, 3, 361, 52, 52, 52 },
{ 128, 4, 362, 52, 52, 52 },
{ 128, 5, 363, 52, 52, 52 },
{ 128, 6, 364, 52, 52, 52 },
{ 128, 0, 365, 53, 52, 52 },
{ 129, 1, 0, 0, 1, 1 },
{ 129, 2, 1, 0, 1, 1 },
{ 129, 3, 2, 0, 1, 1 },
{ 129, 4, 3, 0, 1, 1 },
{ 129, 5, 4, 0, 1, 1 },
{ 129, 6, 5, 0, 1, 1 },
{ 129, 0, 6, 1, 1, 1 },
{ 129, 1, 357, 51, 52, 52 },
{ 129, 2, 358, 51, 52, 52 },
{ 129, 3, 359, 51, 52, 52 },
{ 129, 4, 360, 51, 52, 52 },
{ 129, 5, 361, 51, 52, 52 },
{ 129, 6, 362, 51, 52, 52 },
{ 129, 0, 363, 52, 52, 52 },
{ 129, 1, 364, 52, 1, 53 },
{ 130, 2, 0, 0, 1, 0 },
{ 130, 3, 1, 0, 1, 0 },
{ 130, 4, 2, 0, 1, 0 },
{ 130, 5, 3, 0, 1, 0 },
{ 130, 6, 4, 0, 1, 0 },
{ 130, 0, 5, 1, 1, 0 },
{ 130, 1, 6, 1, 2, 1 },
{ 130, 2, 357, 51, 52, 51 },
{ 130, 3, 358, 51, 52, 51 },
{ 130, 4, 359, 51, 52, 51 },
{ 130, 5, 360, 51, 52, 51 },
{ 130, 6, 361, 51, 52, 51 },
{ 130, 0, 362, 52, 52, 51 },
{ 130, 1, 363, 52, 1, 52 },
{ 130, 2, 364, 52, 1, 52 },
{ 131, 3, 0, 0, 1, 0 },
{ 131, 4, 1, 0, 1, 0 },
{ 131, 5, 2, 0, 1, 0 },
{ 131, 6, 3, 0, 1, 0 },
{ 131, 0, 4, 1, 1, 0 },
{ 131, 1, 5, 1, 2, 1 },
{ 131, 2, 6, 1, 2, 1 },
{ 131, 3, 357, 51, 52, 51 },
{ 131, 4, 358, 51, 52, 51 },
{ 131, 5, 359, 51, 52, 51 },
{ 131, 6, 360, 51, 52, 51 },
{ 131, 0, 361, 52, 52, 51 },
{ 131, 1, 362, 52, 1, 52 },
{ 131, 2, 363, 52, 1, 52 },
{ 131, 3, 364, 52, 1, 52 },
{ 132, 4, 0, 0, 1, 0 },
{ 132, 5, 1, 0, 1, 0 },
{ 132, 6, 2, 0, 1, 0 },
{ 132, 0, 3, 1, 1, 0 },
{ 132, 1, 4, 1, 2, 1 },
{ 132, 2, 5, 1, 2, 1 },
{ 132, 3, 6, 1, 2, 1 },
{ 132, 5, 358, 51, 52, 51 },
{ 132, 6, 359, 51, 52, 51 },
{ 132, 0, 360, 52, 52, 51 },
{ 132, 1, 361, 52, 53, 52 },
{ 132, 2, 362, 52, 53, 52 },
{ 132, 3, 363, 52, 53, 52 },
{ 132, 4, 364, 52, 53, 52 },
{ 132, 5, 365, 52, 53, 52 },
{ 133, 6, 0, 0, 53, 0 },
{ 133, 0, 1, 1, 53, 0 },
{ 133, 1, 2, 1, 1, 1 },
{ 133, 2, 3, 1, 1, 1 },
{ 133, 3, 4, 1, 1, 1 },
{ 133, 4, 5, 1, 1, 1 },
{ 133, 5, 6, 1, 1, 1 },
{ 133, 6, 357, 51, 51, 51 },
{ 133, 0, 358, 52, 51, 51 },
{ 133, 1, 359, 52, 52, 52 },
{ 133, 2, 360, 52, 52, 52 },
{ 133, 3, 361, 52, 52, 52 },
{ 133, 4, 362, 52, 52, 52 },
{ 133, 5, 363, 52, 52, 52 },
{ 133, 6, 364, 52, 52, 52 },
{ 134, 0, 0, 1, 52, 0 },
{ 134, 1, 1, 1, 1, 1 },
{ 134, 2, 2, 1, 1, 1 },
{ 134, 3, 3, 1, 1, 1 },
{ 134, 4, 4, 1, 1, 1 },
{ 134, 5, 5, 1, 1, 1 },
{ 134, 6, 6, 1, 1, 1 },
{ 134, 0, 357, 52, 51, 51 },
{ 134, 1, 358, 52, 52, 52 },
{ 134, 2, 359, 52, 52, 52 },
{ 134, 3, 360, 52, 52, 52 },
{ 134, 4, 361, 52, 52, 52 },
{ 134, 5, 362, 52, 52, 52 },
{ 134, 6, 363, 52, 52, 52 },
{ 134, 0, 364, 53, 52, 52 },
{ 135, 1, 0, 0, 1, 1 },
{ 135, 2, 1, 0, 1, 1 },
{ 135, 3, 2, 0, 1, 1 },
{ 135, 4, 3, 0, 1, 1 },
{ 135, 5, 4, 0, 1, 1 },
{ 135, 6, 5, 0, 1, 1 },
{ 135, 0, 6, 1, 1, 1 },
{ 135, 1, 357, 51, 52, 52 },
{ 135, 2, 358, 51, 52, 52 },
{ 135, 3, 359, 51, 52, 52 },
{ 135, 4, 360, 51, 52, 52 },
{ 135, 5, 361, 51, 52, 52 },
{ 135, 6, 362, 51, 52, 52 },
{ 135, 0, 363, 52, 52, 52 },
{ 135, 1, 364, 52, 1, 53 },
{ 136, 2, 0, 0, 1, 0 },
{ 136, 3, 1, 0, 1, 0 },
{ 136, 4, 2, 0, 1, 0 },
{ 136, 5, 3, 0, 1, 0 },
{ 136, 6, 4, 0, 1, 0 },
{ 136, 0, 5, 1, 1, 0 },
{ 136, 1, 6, 1, 2, 1 },
{ 136, 3, 358, 51, 52, 51 },
{ 136, 4, 359, 51, 52, 51 },
{ 136, 5, 360, 51, 52, 51 },
{ 136, 6, 361, 51, 52, 51 },
{ 136, 0, 362, 52, 52, 51 },
{ 136, 1, 363, 52, 1, 52 },
{ 136, 2, 364, 52, 1, 52 },
{ 136, 3, 365, 52, 1, 52 },
{ 137, 4, 0, 0, 1, 0 },
{ 137, 5, 1, 0, 1, 0 },
{ 137, 6, 2, 0, 1, 0 },
{ 137, 0, 3, 1, 1, 0 },
{ 137, 1, 4, 1, 2, 1 },
{ 137, 2, 5, 1, 2, 1 },
{ 137, 3, 6, 1, 2, 1 },
{ 137, 4, 357, 51, 52, 51 },
{ 137, 5, 358, 51, 52, 51 },
{ 137, 6, 359, 51, 52, 51 },
{ 137, 0, 360, 52, 52, 51 },
{ 137, 1, 361, 52, 53, 52 },
{ 137, 2, 362, 52, 53, 52 },
{ 137, 3, 363, 52, 53, 52 },
{ 137, 4, 364, 52, 53, 52 },
};

static int test_week_calc( void )
{
    char buffer[100];
    int rc = 1;
    for ( int i = 0; i < 1020; ++i )
    {
        struct tm t = { 0 };
        t.tm_year = data[i][0];
        t.tm_wday = data[i][1];
        t.tm_yday = data[i][2];
        assert( strftime( buffer, 100, "%U %V %W", &t ) == 8 );
        int U, V, W;
        assert( sscanf( buffer, "%d %d %d", &U, &V, &W ) == 3 );
        if ( data[i][3] != U || data[i][4] != V || data[i][5] != W )
        {
            printf( "Fehler in { %d, %d, %d, %d, %d, %d } (encountered { %d, %d, %d })\n", data[i][0], data[i][1], data[i][2], data[i][3], data[i][4], data[i][5], U, V, W );
            rc = 0;
        }
    }
    return rc;
}

int main( void )
{
    char buffer[100];
    /* Basic functionality */
    struct tm timeptr;
    MKTIME( timeptr, 59, 30, 12, 1, 9, 72, 0, 274 );
    TESTCASE( strftime( buffer, 100, "%a ", &timeptr ) == 4 );
    TESTCASE( strcmp( buffer, "Sun " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%A ", &timeptr ) == 7 );
    TESTCASE( strcmp( buffer, "Sunday " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%b ", &timeptr ) == 4 );
    TESTCASE( strcmp( buffer, "Oct " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%h ", &timeptr ) == 4 );
    TESTCASE( strcmp( buffer, "Oct " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%B ", &timeptr ) == 8 );
    TESTCASE( strcmp( buffer, "October " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%c ", &timeptr ) == 25 );
    TESTCASE( strcmp( buffer, "Sun Oct  1 12:30:59 1972 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%C ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "19 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%d ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "01 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%D ", &timeptr ) == 9 );
    TESTCASE( strcmp( buffer, "10/01/72 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%e ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, " 1 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%F ", &timeptr ) == 11 );
    TESTCASE( strcmp( buffer, "1972-10-01 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%H ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "12 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%I ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "12 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%j ", &timeptr ) == 4 );
    TESTCASE( strcmp( buffer, "275 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%m ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "10 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%M ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "30 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%p ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "PM " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%r ", &timeptr ) == 12 );
    TESTCASE( strcmp( buffer, "12:30:59 PM " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%R ", &timeptr ) == 6 );
    TESTCASE( strcmp( buffer, "12:30 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%S ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "59 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%T ", &timeptr ) == 9 );
    TESTCASE( strcmp( buffer, "12:30:59 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%u ", &timeptr ) == 2 );
    TESTCASE( strcmp( buffer, "7 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%w ", &timeptr ) == 2 );
    TESTCASE( strcmp( buffer, "0 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%x ", &timeptr ) == 9 );
    TESTCASE( strcmp( buffer, "10/01/72 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%X ", &timeptr ) == 9 );
    TESTCASE( strcmp( buffer, "12:30:59 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%y ", &timeptr ) == 3 );
    TESTCASE( strcmp( buffer, "72 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%Y ", &timeptr ) == 5 );
    TESTCASE( strcmp( buffer, "1972 " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%% ", &timeptr ) == 2 );
    TESTCASE( strcmp( buffer, "% " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%n ", &timeptr ) == 2 );
    TESTCASE( strcmp( buffer, "\n " ) == 0 );
    TESTCASE( strftime( buffer, 100, "%t ", &timeptr ) == 2 );
    TESTCASE( strcmp( buffer, "\t " ) == 0 );
    TESTCASE( test_week_calc() );
    return TEST_RESULTS;
}

#endif
