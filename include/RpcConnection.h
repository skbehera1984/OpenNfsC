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

#ifndef _RPC_CONNECTION_H_
#define _RPC_CONNECTION_H_

#include "BasicConnection.h"
#include <Thread.h>
#include <arpa/inet.h>
#include <map>

namespace OpenNfsC {

class Responder;
class RpcPacket;

class RpcConnection: public BasicConnection
{
  typedef uint32 Xid;
  public:
    // synchronous send and wait. will block
    virtual int sendAndWait(RpcPacketPtr, RpcPacketPtr& reply, int timeout_ms);

    virtual int writePacket(RpcPacketPtr);

    // used by connection manager thread to recv replies
    virtual int recvPacket();
    virtual int closeConnection();
    uint32 getNextXid();

    // create a request
    // input: procedure number
    // output: a RpaPacket ref pointer to created request
    virtual RpcPacketPtr createRequest(int);
    virtual std::string toString() const;

    static BasicConnectionPtr create(ConnKey& connKey, uint32 prog, uint32 version, bool resvPort = false);
    static void clear(BasicConnectionPtr& ptr);
    ~RpcConnection();

  private:
    RpcConnection();  // not implemented
    RpcConnection(const RpcConnection&); // not implemented
    RpcConnection& operator=(const RpcConnection&); //not implemented;
    RpcConnection(ConnKey& connKey, uint32 prog, uint32 version, bool resvPort);

    int recvUDPPacket(RpcPacketPtr& reply);
    int recvTCPPacket(RpcPacketPtr& reply);

    Responder* findPendingRequest(RpcPacketPtr);
    bool addPendingRequest( RpcPacketPtr, Responder* responder);
    bool removePendingRequest(RpcPacketPtr);
    bool matchPendingRequest(RpcPacketPtr); // match reply with pending request

    int cleanup();

    int doSendAndWait(RpcPacketPtr, RpcPacketPtr& reply, int timeout_ms);

  private: //data members
    static uint32 salt;
    uint32 m_nextXid;
    uint32 m_progNumber;
    uint32 m_version;

    struct Fragment
    {
      bool isHeaderDone;
      uint32 header;  //network order
      bool isDataDone;
      uint32 length;
      RpcPacketPtr m_packet;
      Fragment():isHeaderDone(false), header(0), isDataDone(0), length(0), m_packet(NULL){}
      bool isLastFragment() { return !((ntohl(header) & 0x80000000) == 0); }
      bool isIncompleteHeader() { return !isHeaderDone; }
      bool isIncompleteFragment() { return !isDataDone; }
      void clear() {isHeaderDone= false; header = 0;  isDataDone = 0; length = 0; m_packet = NULL; }
    };

    class SemaphoreGuard
    {
      public:
        SemaphoreGuard(); //not implemented
        SemaphoreGuard(Thread::Semaphore& sem) : m_pSem(&sem) { m_pSem->acquire(); }
        SemaphoreGuard(Thread::Semaphore* psem) : m_pSem(psem) { m_pSem->acquire(); }
        ~SemaphoreGuard() { m_pSem->release(); }
      private:
        Thread::Semaphore* m_pSem;
    };

    // internal data member to hold partial message
    Fragment m_fragment;

    // xid table
    Thread::Mutex m_xidTableLock;
    std::map<Xid, Responder*> m_xidTable;

    //allowed concurrent pending requests for TCP
    Thread::Semaphore m_concurrency;
};

}

#endif /* _RPC_CONNECTION_H_ */
