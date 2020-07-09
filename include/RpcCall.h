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

#ifndef _RPC_CALL_
#define _RPC_CALL_

#include "NfsConnectionGroup.h"
#include "RpcDefs.h"
#include <rpc/rpc.h>
#include <SmartPtr.h>
#include "RpcPacket.h"

namespace OpenNfsC {

class NfsConnectionGroup;
class RpcPacket;

class RemoteCall : public SmartRef
{
  public:
    virtual ~RemoteCall();
    RemoteCall(enum ServiceType prog, uint32 proc):m_request(NULL), m_reply(NULL), m_program(prog), m_procedure(proc), m_errno(0) {}
    enum clnt_stat call(const NfsConnectionGroupPtr& groupPtr, int timeout_s=0);
    enum clnt_stat call(NfsConnectionGroup* pConnGroup, int timeout_s=0);
    virtual int encodeArguments() = 0;
    virtual int decodeResults() = 0;
    //! By default for all calls we ensure the existence of the connection
    virtual bool checkForConnection() const { return true; }

    RpcPacketPtr getRequest() { return m_request; }
    RpcPacketPtr getReply() { return m_reply; }

    void clear();

    int getErrno() const { return m_errno; }
    void setErrno(int value) { m_errno = value; }

  private:
    RpcPacketPtr m_request;
    RpcPacketPtr m_reply;

    enum ServiceType m_program;
    uint32 m_procedure;
    int m_errno;
};

} // end of namespace
#endif /* _RPC_CALL_ */
