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

#include "RpcPacket.h"
#include "RpcDefs.h"
#include "NfsUtil.h"

#include <base.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <iostream>
#include <syslog.h>

namespace OpenNfsC {

namespace
{
  RpcPacket* initDefaultUnixAuth()
  {
    RpcPacket* pkt = new RpcPacket(32);
    pkt->xdrEncodeUint32(0); //stamp

    unsigned char host[4] = "fma";
    pkt->xdrEncodeString(host,3);
    pkt->xdrEncodeUint32(0); //uid
    pkt->xdrEncodeUint32(0); //gid
    pkt->xdrEncodeUint32(0); // gid count
    return pkt;
  }

  // a packet contains default unix auth bytes
  RpcPacket* defaultUnixAuth = initDefaultUnixAuth();
}

RpcPacket::RpcPacket(uint32 msgSize):Packet(msgSize), m_header()
{
  syslog(LOG_DEBUG, "RpcPacket::RpcPacket calls %p\n", this);
}

RpcPacket::RpcPacket(RpcHeader& hdr):Packet(), m_header(hdr)
{
  syslog(LOG_DEBUG, "RpcPacket::RpcPacket hdr calls %p\n", this);
}

RpcPacket::~RpcPacket()
{
  syslog(LOG_DEBUG, "RpcPacket::~RpcPacket this=%p xid=%d\n", this, m_header.xid);
}


int RpcPacket::xdrEncodeUint32(uint32 iValue)
{
  uint32 nValue = htonl(iValue);
  return append((unsigned char*)&nValue, (uint32)sizeof(uint32));
}


#if defined(R_ENDIAN_LITTLE)
#define hton64(x) __bswap_64(x)
#define ntoh64(x) __bswap_64(x)
#elif defined(R_ENDIAN_BIG)
#define hton64(x) (x)
#define ntoh64(x) (x)
#endif

int RpcPacket::xdrEncodeUint64(uint64 iValue)
{
  uint64 nValue = hton64(iValue);
  return append((unsigned char*)&nValue, (uint32)sizeof(uint64));
}

int RpcPacket::xdrEncodeFixedOpaque(void* buf, uint32 len )
{
  uint32 padding = 0;
  int padSize = 0;
  if (len%4 != 0)
    padSize = 4 - len%4;
  int nwrite = append((unsigned char*)buf, len);
  if (padSize > 0 && nwrite != -1)
    nwrite = append((unsigned char*)&padding, padSize);
  return nwrite;
}

int RpcPacket::xdrEncodeVarOpaque(void* buf, uint32 len)
{
  int nwrite = xdrEncodeUint32(len);
  if ( nwrite < 0)
    return nwrite;
  else
    return xdrEncodeFixedOpaque(buf, len);
}


int RpcPacket::xdrEncodeString( unsigned char* buf, uint32 len)
{
  return xdrEncodeVarOpaque(buf, len);
}

int RpcPacket::xdrDecodeUint32(uint32 *iPtr)
{
  uint32 nValue;
  if (read((unsigned char*)&nValue, (uint32)sizeof(uint32)) < 0)
    return -1;
  *iPtr = ntohl(nValue);
    return 0;
}


int RpcPacket::xdrDecodeUint64(uint64 *iPtr)
{
  uint64 nValue;
  if (read((unsigned char*)&nValue, (uint32)sizeof(uint64)) < 0)
    return -1;
  *iPtr = ntoh64(nValue);
    return 0;
}

int RpcPacket::xdrDecodeString(unsigned char*& outBufPtr, uint32& outLen, bool copy)
{
  uint32 length = 0;

  if (xdrDecodeUint32(&length) < 0)
    return -1;

  outLen = length;

  if (copy)
  {
    outBufPtr = (unsigned char*)malloc((length)*sizeof(unsigned char*));

    if (read(outBufPtr, length) < 0)
      return -1;
  }
  else
  {
    outBufPtr = getReadAddress();
    if (read(NULL, length) < 0)
      return -1;
  }

  return xdrDecodeSkipPadding(length);
}

int RpcPacket::xdrDecodeFixedOpaque(unsigned char* outbuf, uint32 len)
{
  if (read(outbuf, len) < 0)
    return -1;

  return xdrDecodeSkipPadding(len);
}

int RpcPacket::xdrDecodeSkipPadding(uint32 len)
{
  uint32 padSize = 4 - len%4;
  if (padSize < 4)
  {
    if (skip(padSize) < 0)
      return -1;
  }
  return 0;
}

int RpcPacket::xdrSkipOpaqueAuth()
{
  if (skip(4) < 0) //verify
    return -1;

  uint32 len = 0;
  unsigned char* ptr = NULL;
  return xdrDecodeString(ptr, len, false);
}


int RpcPacket::xdrGetRawData(unsigned char *outData, uint32 size)
{
  return read(outData, size);
}

int RpcPacket::decodeHeader()
{
  RETURN_ON_ERROR(xdrDecodeUint32(&m_header.xid));
  RETURN_ON_ERROR(xdrDecodeUint32(&m_header.call));
  if (m_header.call == RPC_REPLY)
  {
    RETURN_ON_ERROR(xdrDecodeUint32((uint32*)&m_header.cr.r)); //reply status
    if (m_header.cr.r.status == 0)
    {
      RETURN_ON_ERROR(xdrSkipOpaqueAuth());  // verifier
      RETURN_ON_ERROR(skip(4));              // accepted status
      return 0;
    }
  }

  syslog(LOG_DEBUG, "got xid=%ld", (long)m_header.xid);

  return 0;
}


int RpcPacket::encodeHeader()
{
  RETURN_ON_ERROR(xdrEncodeUint32(m_header.xid));
  RETURN_ON_ERROR(xdrEncodeUint32(m_header.call));
  if (m_header.call == RPC_CALL)
  {
    //m_header.cr.proc = rpcInfo;
    RETURN_ON_ERROR(xdrEncodeUint32(2)); // rpc version
    RETURN_ON_ERROR(xdrEncodeUint32(m_header.cr.proc.prog));
    RETURN_ON_ERROR(xdrEncodeUint32(m_header.cr.proc.vers));
    RETURN_ON_ERROR(xdrEncodeUint32(m_header.cr.proc.proc));

    /* cred*/
    RETURN_ON_ERROR(xdrEncodeUint32(AUTHTYPE_UNIX));

    RpcPacket* pkt = defaultUnixAuth;

    if (pkt == NULL)
      return -1;

    RETURN_ON_ERROR(xdrEncodeVarOpaque(pkt->getBuffer(), pkt->getSize())); // auth

    /* verifier */
    RETURN_ON_ERROR(xdrEncodeUint32(AUTHTYPE_NULL));
    RETURN_ON_ERROR(xdrEncodeUint32(0)); // length
  }
  return 0;
}

} // end of namespace
