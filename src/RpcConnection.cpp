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

#include "RpcConnection.h"
#include "RpcPacket.h"
#include "Responder.h"
#include "ConnectionMgr.h"
#include "RpcDefs.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <syslog.h>

#include <nfsrpc/pmap.h>
#include <nfsrpc/nfs.h>
#include <nfsrpc/nlm.h>
#include <nfsrpc/mount.h>

using OpenNfsC::Thread::MutexGuard;

namespace OpenNfsC {

// allowed udp pending requests
#define	NFS_UDP_CONCURRENCY	8
#define	NFS_UDP_RETRIES		6
#define	NFS_UDP_TIMEOUT		10000

// allowed tcp pending requests
#define	NFS_TCP_CONCURRENCY	128
#define	NFS_TCP_TIMEOUT		180000


const int udpRetries = NFS_UDP_RETRIES;// udp retries

static inline int getConcurrencyConfig(const ConnKey& connKey)
{
  int concurrency = (connKey.getTransport() == TRANSP_TCP) ? NFS_TCP_CONCURRENCY : NFS_UDP_CONCURRENCY;
  if (concurrency <= 0 || concurrency > 128)
    concurrency = (connKey.getTransport() == TRANSP_TCP) ? 128 : 8;
  return concurrency;
}

// Initialize salt to 0
uint32 RpcConnection::salt = 0;

BasicConnectionPtr RpcConnection::create(ConnKey& connKey, uint32 prog, uint32 version, bool resvPort)
{
  BasicConnectionPtr ptr;

  try
  {
    ptr = new RpcConnection(connKey, prog, version, resvPort);
    ptr->nonblockConnect();
    syslog(LOG_DEBUG, "RpcConnection::%s: this=%p  %s:%d, fd=%d\n",
           __func__, ptr.ptr(), ptr->getServerIpStr(), ptr->getConnKey().getServerPort(), ptr->getSocketId());
  }
  catch ( RpcConnection* p )
  {
    if ( p ) delete p;
    throw std::string("Unable to create RpcConnection object");
  }

  // ensure that the object is available
  if ( ptr.empty() )
    throw std::string("Unable to create RpcConnection object");

  if ( ptr->getSocketId() > 0 )
    ConnectionMgr::getInstance()->addConnection(ptr);

  return ptr;
}

void RpcConnection::clear(BasicConnectionPtr& ptr)
{
  if ( ! ptr.empty() )
    ConnectionMgr::getInstance()->removeConnection(ptr);
  ptr.clear();
}

RpcConnection::RpcConnection(ConnKey& connKey, uint32 prog, uint32 version, bool resvPort):
    BasicConnection(connKey, resvPort),
    m_nextXid(0),
    m_progNumber(prog),
    m_version(version),
    m_fragment(),
    m_xidTableLock(false),
    m_concurrency(getConcurrencyConfig(connKey))
{
  m_nextXid = salt + ((uint64_t)time(NULL) * 1000) + ((uint32_t)getpid() << 16);
  salt += 0x00ABCDEF;

  syslog(LOG_DEBUG, "RpcConnection::RpcConnection concurrency = %d\n", getConcurrencyConfig(connKey));

  // Only log the following info once
  static bool tInit = false;
  if ( !tInit )
  {
    tInit = true;

    cout << "NFS_UDP_CONCURRENCY : " << NFS_UDP_CONCURRENCY << endl;
    cout << "NFS_UDP_RETRIES : " << NFS_UDP_RETRIES << endl;
    cout << "NFS_UDP_TIMEOUT : " << NFS_UDP_TIMEOUT << endl;
    cout << "NFS_TCP_CONCURRENCY : " << NFS_TCP_CONCURRENCY << endl;
    cout << "NFS_TCP_TIMEOUT : " << NFS_TCP_TIMEOUT << endl;
  }
}

RpcConnection::~RpcConnection()
{
  cleanup();
}

uint32 RpcConnection::getNextXid()
{
  MutexGuard lock(m_xidTableLock);
  return ++m_nextXid;
}

RpcPacketPtr RpcConnection::createRequest(int proc)
{
  RpcHeader hdrInfo;
  RpcInfo procInfo = {m_progNumber, m_version, static_cast<uint32>(proc)};
  hdrInfo.xid = getNextXid();
  hdrInfo.call = 0; //RPC_CALL
  hdrInfo.cr.proc = procInfo;

  RpcPacketPtr request = new RpcPacket(hdrInfo);
  if ( !request.empty() )
  {
    // tcp record
    if (getConnKey().getTransport() == TRANSP_TCP)
    {
      if (request->xdrEncodeUint32(0) < 0)
      {
        syslog(LOG_ERR, "RpcConnection::createRequest failed to encode TCP request\n");
        request = NULL;
        return NULL;
      }
    }

    if (request->encodeHeader()  < 0)
    {
      syslog(LOG_ERR, "RpcConnection::createRequest failed to encode request\n");
      request = NULL;
      return NULL;
    }
  }
  return request;
}

std::string RpcConnection::toString() const
{
  std::ostringstream out;

  out << getServerIpStr() << ":" << getConnKey().getServerPort() << "-";
  switch ( m_progNumber )
  {
  case RPCPROG_NFS:   out << "NFS";   break;
  case RPCPROG_MOUNT: out << "MOUNT"; break;
  case RPCPROG_NLM:   out << "NLM";   break;
  case RPCPROG_PMAP:  out << "PMAP";  break;
  default:            out << "PROG" << m_progNumber; break;
  }
  out << "_V" << m_version << "@";
  switch ( getConnKey().getTransport() )
  {
  case TRANSP_TCP: out << "TCP"; break;
  case TRANSP_UDP: out << "UDP"; break;
  default:         out << "PROT" << getConnKey().getTransport(); break;
  }

  return out.str();
}

int RpcConnection::writePacket(RpcPacketPtr pkt)
{
  if ( pkt.empty() )
    return -1;

  uint32 packetSize = pkt->getSize();

  if ( getConnKey().getTransport() == TRANSP_TCP )
  {
    uint32 tcpSize = htonl((packetSize-4) | 0x80000000);
    int ret = pkt->writeOffset((unsigned char*)&tcpSize, sizeof(tcpSize), 0);
    if (ret <= 0)
    {
      cout << "RpcConnection::writePacket:Failed to Wtire packet to socket" << endl;
      abort();
    }
  }

  return BasicConnection::writePacket(pkt);
}

int RpcConnection::recvPacket()
{
  RpcPacketPtr reply;
  int outcome = 0;
  if ( getConnKey().getTransport() == TRANSP_UDP )
  {
    outcome = recvUDPPacket(reply);
  }
  else if (getConnKey().getTransport() == TRANSP_TCP)
  {
    outcome = recvTCPPacket(reply);
  }

  // partial message or empty message
  if (outcome == 1)
    return 1;

  if ( !reply.empty() )
  {
    if (reply->decodeHeader() >= 0)
    {
      // check if there is a pending request matching reply
      bool foundMatch = matchPendingRequest(reply);
      if (foundMatch)
      {
        syslog(LOG_DEBUG, "RpcConnection::recvPacket responder found for reply xid = %d\n", reply->getXid());
      }
      else
      {
        syslog(LOG_ERR, "RpcConnection::recvPacket responder not found for reply xid = %d\n", reply->getXid());
      }
      return 0;
    }
  }

  return -1;
}

int RpcConnection::recvUDPPacket(RpcPacketPtr& reply)
{
  int skt = getSocketId();
  if ( skt == -1 )
    return -1;

  if ( getConnKey().getTransport() == TRANSP_UDP )
  {
    int len = 0;
    if ( ioctl(skt, SIOCINQ, &len) != 0 )
      return -1;

    if ( len == 0 )
    {
      char aByte;
      if ( recv(skt, &aByte, sizeof(aByte), MSG_PEEK|MSG_TRUNC) == 0 )
      {
        recv(skt, &aByte, sizeof(aByte), MSG_TRUNC);
      }

      return 1;
    }

    RpcPacketPtr response = new RpcPacket(len);
    if ( !response.empty() )
    {
      int nread = recv(skt, response->getBuffer(), len, MSG_TRUNC|MSG_NOSIGNAL);
      if ( nread > 0 )
        response->append(NULL, nread);

      if ( nread == len )
      {
        reply = response;
        return 0;
      }
    }
  }
  return -1;
}

int RpcConnection::recvTCPPacket(RpcPacketPtr& reply)
{
  do
  {
    if ( m_fragment.isIncompleteHeader())
    {
      if (!m_readState.isInitialized())
        m_readState.initialize(&m_fragment.header, sizeof(uint32));
      int outcome = recvNbyte();
      if (outcome < 0)
      {
        m_fragment.clear();
      }
      if (outcome != 0)
        return outcome;
      syslog(LOG_DEBUG, "RpcConnection::recvTCPPacket header is done\n");
      m_fragment.length = ntohl(m_fragment.header) & ~0x80000000;
      m_fragment.isHeaderDone = true;
      m_readState.reset();
      if ( m_fragment.m_packet == NULL )
      {
        m_fragment.m_packet = new RpcPacket(m_fragment.length);
        syslog(LOG_DEBUG, "recvTCPPacket intialize packet length %d\n", m_fragment.length);
      }
      else
      {
        syslog(LOG_DEBUG, "recvTCPPacket enlarge packet length %d\n", m_fragment.length);
        m_fragment.m_packet->reserve( m_fragment.length);
      }
    }

    if ( m_fragment.isIncompleteFragment() )
    {
      if (!m_readState.isInitialized())
        m_readState.initialize(m_fragment.m_packet->getBufferEnd(), m_fragment.length);
      int outcome = recvNbyte();
      if (outcome < 0)
      {
        m_fragment.clear();
        m_readState.reset();
      }
      if (outcome != 0)
        return outcome;

      m_fragment.m_packet->append(NULL, m_fragment.length);
      m_readState.reset();
    }

    m_fragment.isHeaderDone = 0; //complete fragment
    m_fragment.isDataDone = 0;
  } while( !m_fragment.isLastFragment() );

  reply = m_fragment.m_packet;
  syslog(LOG_DEBUG, "got complete reply %d\n", m_fragment.length);
  m_fragment.clear();
  return 0;
}

int RpcConnection::sendAndWait(RpcPacketPtr request, RpcPacketPtr& reply, int timeout_ms)
{
  SemaphoreGuard sem(m_concurrency);
  return doSendAndWait(request, reply, timeout_ms);
/*
  int retVal = ensureConnection();
  if ( retVal == 0 )
  {
    SemaphoreGuard sem(m_concurrency);
    retVal = doSendAndWait(request, reply, timeout_ms);
  }
  return retVal;
*/
}

int RpcConnection::doSendAndWait(RpcPacketPtr request, RpcPacketPtr& reply, int timeout_ms)
{
  if ( request.empty() )
    return -1;

  if( findPendingRequest(request) != NULL )
  {
    syslog(LOG_ERR, "RpcConnection::sendAndWait(%s) request %d already exists\n", toString().c_str(), request->getXid());
    return -1;
  }

  Responder* responder = new Responder;
  addPendingRequest(request, responder);

  syslog(LOG_DEBUG, "RpcConnection::sendAndWait send request xid=%d\n", request->getXid());

  int retries = 0;
  int timeout = timeout_ms;

  // for udp, retry twice if time out
  if (getConnKey().getTransport() == TRANSP_UDP)
  {
    retries = udpRetries;
    if (timeout <= 0)
    {
      timeout =  NFS_UDP_TIMEOUT;
    }
  }
  else
  {
    if (timeout <= 0)
    {
      timeout = NFS_TCP_TIMEOUT;
    }
  }

  int outcome = 0;

  do
  {
    if ( writePacket(request) < 0 )
    {
      syslog(LOG_ERR, "RpcConnection::sendAndWait(%s) failed to send request xid = %d\n",
             toString().c_str(),  request->getXid());
    }
    else
    {
      responder->waitForResponse(timeout);
      reply =  responder->getReply();

      int outcome = 0;
      if ( reply.empty() )
      {
        syslog(LOG_ERR, "RpcConnection::SendAndWait(%s): failed to get reply for request xid=%d retry=%d\n",
               toString().c_str(),  request->getXid(), retries);
        outcome = -1;
      }
      else
      {
        syslog(LOG_DEBUG, "RpcConnection::sendAndWait(%s) got reply xid=%d\n",
               toString().c_str(), request->getXid());
        break;
      }
    }
    --retries;
  } while( retries > 0 );

  removePendingRequest(request);
  delete responder;

  return outcome;
}

Responder* RpcConnection::findPendingRequest(RpcPacketPtr request)
{
  Xid xid = request->getXid();
  Responder* found = NULL;
  MutexGuard lock(m_xidTableLock);
  std::map<Xid,Responder*>::iterator iter = m_xidTable.find(xid);
  if (iter != m_xidTable.end() )
  {
    found = iter->second;
  }
  return found;
}

bool RpcConnection::matchPendingRequest(RpcPacketPtr reply)
{
  bool found = false;
  Xid xid = reply->getXid();
  Responder* resp = NULL;

  {
    MutexGuard lock(m_xidTableLock);
    std::map<Xid,Responder*>::iterator iter = m_xidTable.find(xid);
    if (iter != m_xidTable.end() )
    {
      if (iter->second)
      {
        resp = iter->second;
        found = true;
      }
    }
  }
  if (found && resp)
    resp->signalReady(reply);

  return found;
}


bool RpcConnection::addPendingRequest(RpcPacketPtr request, Responder* responder)
{
  if ( request.empty() )
    return false;

  Xid xid = request->getXid();
  {
    MutexGuard lock(m_xidTableLock);
    m_xidTable[xid] = responder;
  }
  return true;
}

bool RpcConnection::removePendingRequest(RpcPacketPtr request)
{
  if ( request.empty() )
    return false;

  Xid xid = request->getXid();
  {
    MutexGuard lock(m_xidTableLock);
    std::map<Xid,Responder*>::iterator iter = m_xidTable.find(xid);
    if ( iter != m_xidTable.end() )
    {
      m_xidTable.erase(iter);
    }
    else
    {
      syslog(LOG_DEBUG, "RpcConnection::removePendingRequest(%s) Request xid=%d not found\n", toString().c_str(), xid);
    }
  }

  return true;
}

int RpcConnection::cleanup()
{
  syslog(LOG_DEBUG, "RpcConnection::cleanup cleanup pending requests\n");
  std::vector<Responder*> respVec;

  {
    MutexGuard lock(m_xidTableLock);
    std::map<Xid,Responder*>::iterator iter = m_xidTable.begin();
    while (iter != m_xidTable.end())
    {
      if (iter->second)
        respVec.push_back(iter->second);
      ++iter;
    }
    m_xidTable.clear();
  }
  std::vector<Responder*>::iterator vecIt;
  for(vecIt = respVec.begin(); vecIt < respVec.end(); vecIt++)
  {
    Responder* resp = *vecIt;
    if(resp)
      resp->signalReady(NULL);
  }

  m_fragment.clear();
  return 0;
}

int RpcConnection::closeConnection()
{
  // close socket
  int res = BasicConnection::disconnect();
  syslog(LOG_DEBUG, "BasicConnection::disconnect() called on %s\n", toString().c_str());

  //cleanup pending requests
  cleanup();
  return res;
}

} /* namespace OpenNfsC */
