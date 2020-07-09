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

#ifndef _RPC_PACKET_
#define _RPC_PACKET_

#include "Packet.h"

namespace OpenNfsC {

class RpcPacket;
typedef SmartPtr<RpcPacket> RpcPacketPtr;

struct RpcInfo
{
  uint32 prog;
  uint32 vers;
  uint32 proc;
};

struct RpcHeader
{
  uint32 xid;
  uint32 call;
  /* need to htonl these */
  union
  {
    RpcInfo proc;
    struct
    {
      uint32 status;
    } r;
  } cr;
};

struct RpcReplyHeader: public RpcHeader
{
    uint32 status;
};

class RpcPacket: public Packet
{
  public:
    RpcPacket(uint32 size);
    RpcPacket(RpcHeader& hdr);
    virtual ~RpcPacket();
    uint32 getXid() { return m_header.xid; }
    void setXid(uint32 xid) { m_header.xid = xid; }
    void setMessageType(uint32 call) { m_header.call = call; }

    int encodeHeader();
    int decodeHeader();
    bool isReplyValid() { return m_header.cr.r.status == 0; }

  public:
    int xdrEncodeUint8(uint8 iValue) {return xdrEncodeUint32((uint32)iValue);}
    int xdrEncodeUint16(uint16 iValue) {return xdrEncodeUint32((uint32)iValue);}
    int xdrEncodeUint32(uint32);
    int xdrEncodeUint64(uint64);
    int xdrEncodeFixedOpaque(void*, uint32);
    int xdrEncodeVarOpaque(void*, uint32);
    int xdrEncodeString(unsigned char*, uint32);

    int xdrDecodeSkipPadding(uint32 len);
    int xdrDecodeUint32(uint32 *iPtr);
    int xdrDecodeUint64(uint64 *iPtr);
    int xdrDecodeString(unsigned char*&, uint32&, bool copy=false);
    int xdrDecodeFixedOpaque(unsigned char*, uint32);
    int xdrSkipOpaqueAuth();
    int xdrGetRawData(unsigned char *outData, uint32 size);

  private:
    RpcHeader m_header;
};

} // end of namespace
#endif /* _RPC_PACKET_ */
