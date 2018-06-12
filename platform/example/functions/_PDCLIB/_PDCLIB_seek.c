/* int64_t _PDCLIB_seek( FILE *, int64_t, int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

/* This is an example implementation of _PDCLIB_seek() fit for use with POSIX
   kernels.
 */

#include <stdio.h>

#ifndef REGTEST

#include "_PDCLIB_glue.h"

#include "/usr/include/errno.h"

extern _PDCLIB_int64_t lseek64( int fd, _PDCLIB_int64_t offset, int whence );
extern long lseek( int fd, long offset, int whence );

_PDCLIB_int64_t _PDCLIB_seek( struct _PDCLIB_file_t * stream, _PDCLIB_int64_t offset, int whence )
{
    switch ( whence )
    {
        case SEEK_SET:
        case SEEK_CUR:
        case SEEK_END:
            /* EMPTY - OK */
            break;
        default:
            /* See comments on implementation-defined errno values in
               <_PDCLIB_config.h>.
            */
            _PDCLIB_errno = _PDCLIB_ERROR;
            return EOF;
            break;
    }
#ifdef __CYGWIN__
    _PDCLIB_int64_t rc = lseek( stream->handle, offset, whence );
#else
    _PDCLIB_int64_t rc = lseek64( stream->handle, offset, whence );
#endif
    if ( rc != EOF )
    {
        stream->ungetidx = 0;
        stream->bufidx = 0;
        stream->bufend = 0;
        stream->pos.offset = rc;
        return rc;
    }
    switch ( errno )
    {
        case EBADF:
        case EFAULT:
            /* See comments on implementation-defined errno values in
               <_PDCLIB_config.h>.
            */
            _PDCLIB_errno = _PDCLIB_ERROR;
            break;
        default:
            /* This should be something like EUNKNOWN. */
            _PDCLIB_errno = _PDCLIB_ERROR;
            break;
    }
    return EOF;
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

int main( void )
{
    /* Testing covered by ftell.c */
    return TEST_RESULTS;
}

#endif
