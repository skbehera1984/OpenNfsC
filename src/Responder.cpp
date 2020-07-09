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

#include "Responder.h"
#include "RpcPacket.h"
#include <sys/time.h>
#include <syslog.h>

namespace OpenNfsC {

using Thread::Mutex;
using Thread::ConditionVar;
using Thread::MutexGuard;

const int defaultTimeOut = 5000;  // 5 seconds

Responder::Responder():m_done(false), m_wait(false), m_condMutex(false), m_cond(&m_condMutex), m_response(NULL)
{
}

void Responder::waitForResponse(int timeoutVal)
{
  if (timeoutVal < defaultTimeOut)
    timeoutVal = defaultTimeOut;

  struct timespec absTimeout;
  ConditionVar::reltime_to_abs(timeoutVal, &absTimeout);

  bool isTimeout;
  m_condMutex.acquire();
  if (!m_done)
  {
    m_wait = true;
    m_response = NULL;
    m_cond.waitAbs(&absTimeout, &isTimeout);
    m_wait = false;
  }
  m_condMutex.release();
}

void Responder::waitForResponse()
{
  waitForResponse(defaultTimeOut);
}

void Responder::signalReady(RpcPacketPtr response)
{
  m_condMutex.acquire();
  m_done = true;
  m_response = response;
  if (m_wait)
    m_cond.signal();

  if (response)
  {
    syslog(LOG_DEBUG, "Responder::signalReady got response %d\n", response->getXid());
  }
  else
  {
    syslog(LOG_DEBUG, "Responder::signalReady got NULL response \n");
  }

  m_condMutex.release();
}

RpcPacketPtr Responder::getReply()
{
  MutexGuard lock(m_condMutex);
  return m_response;
}

} // namespace OpenNfsC

