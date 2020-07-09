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

#ifndef _PACKET_
#define _PACKET_

#include "ByteBuffer.h"
#include <SmartPtr.h>

namespace OpenNfsC {

class Packet : public SmartRef
{
  public:
    Packet(uint32 size=1024):m_buffer(size),m_writeIndex(0),m_readIndex(0){};
    virtual ~Packet() {};

    virtual int decodeHeader() = 0;
    virtual int encodeHeader() = 0;

    int append(unsigned char* inBuf, uint32 len )
    { return m_buffer.append(inBuf,len); }

    /*
    int write(unsigned char* inBuf, uint32 len )
    {
      int res = m_buffer.write(m_writeIndex, inBuf, len);
      if (res > 0)
        m_writeIndex += len;
      return res;
    }
    */

    int read(unsigned char* outBuf, uint32 len)
    {
      int res =  m_buffer.read(m_readIndex, outBuf, len);
      if (res > 0)
        m_readIndex += len;
      return res;
    }

    int reserve(uint32 extraLen)
    {
      return m_buffer.reserve(extraLen);
    }

    int skip(uint32 len) { return read(NULL, len); }

    int readOffset(unsigned char* outBuf, uint32 len, uint32 offset)
    {
      return m_buffer.read(offset, outBuf, len);
    }

    int writeOffset(unsigned char* inBuf, uint32 len, uint32 offset)
    {
      return m_buffer.write(offset, inBuf, len);
    }
    virtual unsigned char* getBuffer() { return m_buffer.begin(); }
    virtual uint32 getSize() { return m_buffer.size(); }
    uint32 getCapacity() { return m_buffer.capacity(); }
    unsigned char* getBufferEnd() { return m_buffer.end(); }

    unsigned char* getReadAddress() { return m_buffer.seek(m_readIndex); }
    uint32 getUnparsedDataSize() { return m_buffer.size() - m_readIndex; }

    //unsigned char* seek(uint32 offset) { return m_buffer.seek(offset); }
    unsigned char* getWriteAddress() { return m_buffer.seek(m_writeIndex); }
    uint32 getWriteSize() { return m_buffer.size() - m_writeIndex; }
    void advanceWriteIndex(uint32 nWrite) { m_writeIndex += nWrite; }
    bool isWriteComplete() { return m_writeIndex == m_buffer.size(); }

    void resetWriteIndex() { m_writeIndex = 0; }
  protected:
    void resetReadIndex()  { m_readIndex = 0;  }

  private:
    // internal data buffer
    Buffer m_buffer;

    // write index: offset of the next send
    uint32 m_writeIndex;

    // read index: used to parse packet
    uint32 m_readIndex;
};

} // end of namespace
#endif /* _PACKET_ */
