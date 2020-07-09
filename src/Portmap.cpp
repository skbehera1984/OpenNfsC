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

#include "Portmap.h"
#include "RpcPacket.h"
#include "NfsUtil.h"
#include "RpcConnection.h"

namespace OpenNfsC {
namespace Portmap {

DumpCall::~DumpCall()
{
  if ( m_dumpList.data )
    delete [] m_dumpList.data;
}

int DumpCall::decodeResults()
{
  RpcPacketPtr reply = getReply();
  if ( reply == NULL )
    return -1;

  int len = reply->getUnparsedDataSize()/(4*5);
  PortmapDumpData* dumpList = new PortmapDumpData[len];

  uint32 cont = 0;
  RETURN_ON_ERROR(reply->xdrDecodeUint32(&cont));

  int i = 0;
  while ( cont && i < len )
  {
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&dumpList[i].program));
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&dumpList[i].version));
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&dumpList[i].transp));
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&(dumpList[i].port)));
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&cont));
    i++;
  }

  m_dumpList.data = dumpList;
  m_dumpList.len = len;
  return 0;
}

}}
