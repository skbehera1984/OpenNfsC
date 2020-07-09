/*
   Copyright (C) 2020 by Sarat Kumar Behera <beherasaratkumar@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _STD_TYPES_H
#define _STD_TYPES_H

#include <sys/types.h>

typedef char int8;
typedef unsigned char uint8;

typedef short int16;
typedef unsigned short uint16;

typedef int int32;
typedef unsigned int uint32;

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffU       /* That's 8 f's */
#define INT32_MAX  0x7fffffff       /* That's 7 f's */
#endif

typedef long long int64;
typedef unsigned long long uint64;

#ifndef UINT64_MAX
#define UINT64_MAX 0xffffffffffffffffULL /* That's 16 f's */
#define INT64_MAX  0x7fffffffffffffffLL /* That's 15 f's */
#endif

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

typedef uint8 BOOL;

/* macros to get the higher and lower address (not order) d's of a q */
union _uint64_parts {
  uint64 n;
  struct {
    int32 x;
    int32 y;
  } p;
};
#define low32of64(V)    ({ union _uint64_parts _v; _v.n = V; _v.p.x; })
#define high32of64(V)   ({ union _uint64_parts _v; _v.n = V; _v.p.y; })

#endif /* _STD_TYPES_H */
