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

#ifndef _BASE_H
#define _BASE_H

/*
verify platform
*/
#ifdef __linux__
# define R_OS_LINUX
#endif

#ifdef __FreeBSD__
# define R_OS_FREEBSD
#endif

#if !defined(R_OS_SOLARIS) && \
  !defined(R_OS_LINUX) && \
  !defined(R_OS_FREEBSD) && \
  !defined(R_OS_WIN_NT) && \
  !defined(R_OS_WIN_2000) && \
  !defined(R_OS_VXWORKS) && \
  !defined(R_OS_PHAR_LAP)
  #error "Platform undefined"
#endif


/*
If R_OS_SOLARIS, R_OS_LINUX, R_OS_FREEBSD or R_OS_VXWORKS is defined, R_UNIX will be defined.
If R_OS_WIN_NT or R_OS_WIN_2000 are defined, R_WIN will be defined.
*/

#if defined(R_OS_SOLARIS) || defined(R_OS_LINUX) || defined(R_OS_FREEBSD)
  #define R_UNIX
#elif defined(R_OS_VXWORKS)
  /* VxWorks isn't UNIX, but its programming model follows UNIX's closely */
  #define R_UNIX
#elif defined(R_OS_WIN_NT) || defined(R_OS_WIN_2000) || defined(R_OS_PHAR_LAP)
  #define R_WIN
#endif


/*
 * verify compiler
 */
#ifdef __GNUC__
# define R_CC_GPP
#endif

#if !defined(R_CC_GPP) && !defined(R_CC_SUNCC) && !defined(R_CC_MSVC)
  #error "Compiler undefined"
#endif


/*
 * verify endian
 */
#ifdef R_OS_LINUX
#ifndef __KERNEL__
# include <endian.h>
#else
# include <asm/byteorder.h>
#endif
#endif

#ifdef R_OS_FREEBSD
#include <sys/endian.h>
#endif

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN
# define R_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
# define R_ENDIAN_BIG
#elif defined(_BYTE_ORDER) && _BYTE_ORDER == _LITTLE_ENDIAN
# define R_ENDIAN_LITTLE
#elif defined(_BYTE_ORDER) && _BYTE_ORDER == _BIG_ENDIAN
# define R_ENDIAN_BIG
#elif defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
# define R_ENDIAN_LITTLE
#elif !defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)
# define R_ENDIAN_BIG
#endif

#if !defined(R_ENDIAN_BIG) && !defined(R_ENDIAN_LITTLE)
  #error "Endian undefined"
#endif


/*
 * A macro will be defined for whether we're running in a multithreaded environment.
*/
#ifdef R_WIN
  #ifdef _MT
    #define R_MULTITHREADED
  #endif
#elif defined(R_UNIX)
  #ifdef _REENTRANT
    #define R_MULTITHREADED
  #endif
#endif


/*
 * Define a macro to indicate that namespaces are supported.
 */
#ifndef R_OS_VXWORKS
  #define R_NAMESPACE_SUPPORTED
#endif


#ifdef R_OS_LINUX
  #define R_HAVE_POLL
#endif


//Compiler specific mods
#ifdef R_CC_MSVC

  #pragma warning (disable : 4518)

  /* don't warn with symbols exceeding 255 chars in length */
  #pragma warning (disable : 4786)

#endif


//VxWorks-specific includes
#ifdef R_OS_VXWORKS
#include "vxWorks.h"
#endif

#ifdef R_WIN
#ifndef R_OS_PHAR_LAP
  #pragma comment(exestr, COPYRIGHT_STRING)
#endif
#endif

#endif /* _BASE_H */
