/* Copyright (C) 1991, 1992, 1996, 1998 Free Software Foundation, Inc.
   This file is derived from mkstemp.c from the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA  */

#include "config.h"
#include "wine/port.h"

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifndef TMP_MAX
#define TMP_MAX 16384
#endif

/*

@deftypefn Replacement int mkstemps (char *@var{template}, int @var{suffix_len})

Generate a unique temporary file name from @var{template}.
@var{template} has the form:

@example
   @var{path}/ccXXXXXX@var{suffix}
@end example

@var{suffix_len} tells us how long @var{suffix} is (it can be zero
length).  The last six characters of @var{template} before @var{suffix}
must be @samp{XXXXXX}; they are replaced with a string that makes the
filename unique.  Returns a file descriptor open on the file for
reading and writing.

@end deftypefn

*/

int
mkstemps (
     char *template,
     int suffix_len)
{
  static const char letters[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static unsigned __int64 value;
  char *XXXXXX;
  size_t len;
  int count;

  len = strlen (template);

  if ((int) len < 6 + suffix_len
      || strncmp (&template[len - 6 - suffix_len], "XXXXXX", 6))
    {
      return -1;
    }

  XXXXXX = &template[len - 6 - suffix_len];

#ifndef _WIN32
  {
      struct timeval tv;
      gettimeofday( &tv, NULL );
      value += ((unsigned __int64) tv.tv_usec << 16) ^ tv.tv_sec;
  }
#endif
  value += getpid();

  for (count = 0; count < TMP_MAX; ++count)
    {
      unsigned __int64 v = value;
      int fd;

      /* Fill in the random bits.  */
      XXXXXX[0] = letters[v % 62];
      v /= 62;
      XXXXXX[1] = letters[v % 62];
      v /= 62;
      XXXXXX[2] = letters[v % 62];
      v /= 62;
      XXXXXX[3] = letters[v % 62];
      v /= 62;
      XXXXXX[4] = letters[v % 62];
      v /= 62;
      XXXXXX[5] = letters[v % 62];

      fd = open (template, O_RDWR|O_CREAT|O_EXCL, 0600);
      if (fd >= 0)
	/* The file does not exist.  */
	return fd;

      /* This is a random value.  It is only necessary that the next
	 TMP_MAX values generated by adding 7777 to VALUE are different
	 with (module 2^32).  */
      value += 7777;
    }

  return -1;
}
