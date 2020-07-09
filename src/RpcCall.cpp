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

#include "RpcCall.h"
#include "NfsConnectionGroup.h"
#include "RpcPacket.h"
#include "RpcConnection.h"
#include <iostream>
#include <rpc/rpc.h>
#include <syslog.h>

namespace OpenNfsC {

using Thread::MutexGuard;

RemoteCall::~RemoteCall()
{
  clear();
}

void RemoteCall::clear()
{
  if ( !m_request.empty() )
  {
    syslog(LOG_DEBUG, "RemoteCall::%s: delete request \n", __func__);
    m_request.clear();
  }

  if ( !m_reply.empty() )
  {
    syslog(LOG_DEBUG, "RemoteCall::%s: delete reply \n", __func__);
    m_reply.clear();
  }
}

enum clnt_stat RemoteCall::call(const NfsConnectionGroupPtr& connGroup, int timeout_s)
{
  return call(connGroup.ptr(), timeout_s);
}

enum clnt_stat RemoteCall::call(NfsConnectionGroup* pConnGroup, int timeout_s)
{
  setErrno(0);
  if ( pConnGroup == NULL )
  {
    syslog(LOG_ERR, "RemoteCall::call NfsConnectionGroup is NULL\n");
    return RPC_SYSTEMERROR;
  }

  if ( checkForConnection() && !pConnGroup->ensureConnection() )
  {
    syslog(LOG_ERR, "RemoteCall::call unable to ensure connection\n");
    return RPC_SYSTEMERROR;
  }

  BasicConnectionPtr conn = pConnGroup->getNfsConnection(m_program);
  if ( conn.empty() )
  {
    syslog(LOG_ERR, "RemoteCall::call connection is NULL\n");
    return RPC_SYSTEMERROR;
  }

  m_request = conn->createRequest(m_procedure);
  if ( m_request.empty() )
    return RPC_SYSTEMERROR;

  if ( encodeArguments() < 0 )
    return RPC_CANTENCODEARGS;

  int timeout_ms = timeout_s * 1000;
  int outcome = conn->sendAndWait(m_request, m_reply, timeout_ms);
  setErrno(conn->getErrno());
  if ( outcome < 0 )
    return RPC_CANTRECV;

  if ( m_reply.empty() )
    return RPC_TIMEDOUT;

  if ( m_reply->isReplyValid() )
  {
    if ( decodeResults() < 0 )
    {
      syslog(LOG_ERR, "RemoteCall::call failed to decode reply \n");
      return RPC_CANTDECODERES;
    }
  }

  return RPC_SUCCESS;
}

} // end of namespace
