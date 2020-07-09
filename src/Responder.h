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

#ifndef _RESPONDER_H_
#define _RESPONDER_H_

#include "RpcPacket.h"
#include <Thread.h>

namespace OpenNfsC {

class RpcPacket;

class Responder
{
  public:
    Responder();
    void waitForResponse();
    void waitForResponse(int timeout);
    void signalReady(RpcPacketPtr reply);

    RpcPacketPtr getReply();

  private:
    Responder(const Responder&); // not implemented
    Responder& operator=(const Responder&); //not implemented;

  private:
    bool m_done;
    bool m_wait;
    Thread::Mutex m_condMutex;
    Thread::ConditionVar m_cond;
    RpcPacketPtr m_response;
    struct timespec m_timeout;
};

}

#endif /* _RESPONDER_H_ */
