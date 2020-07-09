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


/* ***************************************
 * A simple buffer that dynamically grows larger as needed
 * **************************************/

#ifndef _BYTE_BUFFER_
#define _BYTE_BUFFER_

#include <stdTypes.h>
#include <string.h>
#include <stdlib.h>

namespace OpenNfsC {

class Buffer
{
  public:
    Buffer();
    Buffer(uint32 size);
    ~Buffer();

    /* append data to buffer. this changes buffer data size.
     * input:
     *      ptr: pointer to data to be appended
     *      len: length of the data to be appended
     * return value:
     *      -1: failure
     *      other: success
     */
    int append(unsigned char* ptr, uint32 len);

    /* read data of len starting at offset. It doesnt change buffer data size */
    int read(uint32 offset, unsigned char* ptr, uint32 len);

    /* write data of len starting at offset It doesnt change buffer data size */
    int write(uint32 offset, const unsigned char* ptr, uint32 len);

    /* resize buffer to newSize */
    bool resize(uint32 newSize);

    /* reserve extra space in buffer */
    bool reserve(uint32 extra) { return resize(size()+extra); }

    /* get the buffer address at the offset */
    unsigned char* seek(uint32 offset);

    /* get buffer data size*/
    uint32 size() { return m_length; }

    /* get allocated buffer size */
    uint32 capacity() { return m_capacity; }

    unsigned char* begin() { return m_buf; }

    /* get end of buffer data */
    unsigned char* end() { return m_buf+m_length; }
    void clear() { m_length = 0; }

  private:
    Buffer(const Buffer& buf); //not implemented
    Buffer& operator=(const Buffer& buf); //not implemented

  private:
    unsigned char* m_buf;  // hold allocated memory
    uint32 m_length;   // data size
    uint32 m_capacity; // allocated buffer size
};


inline Buffer::Buffer():m_buf(NULL),m_length(0),m_capacity(0) {}


inline Buffer::Buffer(uint32 size):m_buf(NULL), m_length(0), m_capacity(0)
{
  if (size != 0)
  {
    m_buf = (unsigned char*)malloc(sizeof(char)*size);
    m_capacity = size;
  }
}

inline Buffer::~Buffer()
{
  clear();

  if (m_buf)
    free(m_buf);
  m_buf = NULL;
  m_capacity = 0;
}

inline bool Buffer::resize(uint32 newSize)
{
  if (newSize <= m_capacity)
    return true;

  uint32 adjustedSize = 2*m_capacity;
  if (newSize > 2*m_capacity)
    adjustedSize = 2*newSize;

  unsigned char* newPtr = (unsigned char*)realloc(m_buf, sizeof(char)*adjustedSize);
  if (newPtr != NULL)
  {
    m_buf = newPtr;
    m_capacity = adjustedSize;
    return true;
  }
  return false;
}

inline int Buffer::append(unsigned char* ptr, uint32 len)
{
  if (m_capacity < (m_length + len))
  {
    if (resize(m_length + len) == false)
      return -1;
  }

  if (ptr != NULL)
    memcpy(end(), ptr, len);
  m_length += len;
  return len;
}

inline int Buffer::read(uint32 offset, unsigned char* outputPtr, uint32 len)
{
  if (offset + len > m_capacity)
    return -1;

  if (outputPtr != NULL)
    memcpy(outputPtr, m_buf+offset, len);
  return len;
}

inline int Buffer::write(uint32 offset, const unsigned char* inputPtr, uint32 len)
{
  if ( inputPtr != NULL && (offset + len) < m_length)
  {
    memcpy(m_buf+offset, inputPtr, len);
    return len;
  }
  return -1;
}

inline unsigned char* Buffer::seek(uint32 offset)
{
  if (offset >= m_capacity)
    return NULL;

  return m_buf+offset;
}

} // end of namespace
#endif /* _BYTE_BUFFER_ */
