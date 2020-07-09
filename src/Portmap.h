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

#ifndef _PORTMAP_H_
#define _PORTMAP_H_

#include "RpcCall.h"
#include <nfsrpc/pmap.h>

namespace OpenNfsC {

namespace Portmap {

struct PortmapDumpData
{
  uint32 program;
  uint32 version;
  uint32 transp;
  uint32 port;
};

struct PortmapDumpList
{
  int len;
  PortmapDumpData* data;
};


class DumpCall : public RemoteCall
{
public:
  DumpCall():RemoteCall(PORTMAP, PMAP_V2_DUMP),m_dumpList() {}
  ~DumpCall();

  const PortmapDumpList& getResult() { return m_dumpList; }

private:
  virtual int encodeArguments() { return 0; }
  virtual int decodeResults();

  //! IMPORTANT: We do not want to check for connection here,
  // because NfsConnectionGroup.cpp's ensureConnection would result in a
  // recursive call
  bool checkForConnection() const override { return false; }

private:
  PortmapDumpList m_dumpList;
};

} // namespace Portmap

} // end of namespace
#endif /* _PORTMAP_H_ */
