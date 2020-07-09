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

#include "BasicConnection.h"
#include "RpcPacket.h"
#include "ConnectionMgr.h"
#include "Stringf.h"

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
#include <syslog.h>

namespace OpenNfsC {

using Thread::MutexGuard;

const time_t gReconnectInterval = 3;

BasicConnection::BasicConnection(ConnKey& key, bool resv):
  m_readState(),
  m_connMutexLock(false),
  m_connKey(key),
  m_reservedPort(resv),
  m_socketId(-1),
  m_connectState(STATE_UNKNOWN),
  m_inConnMgr(false),
  m_lastConnectTime(0),
  m_errno(0)
{
  strcpy(m_serverIPStr,getConnKey().getServerIP().c_str());
  syslog(LOG_DEBUG, "BasicConnection::%s: this=%p  %s:%d, fd=Yet to create\n", __func__, this, m_serverIPStr, getConnKey().getServerPort());
}

BasicConnection::~BasicConnection()
{
  syslog(LOG_DEBUG, "BasicConnection::%s: this=%p  %s:%d, fd=%d\n", __func__, this, m_serverIPStr, getConnKey().getServerPort(),m_socketId);
  disconnect();
}

int BasicConnection::nonblockConnect()
{
  MutexGuard lock(m_connMutexLock);
  return p_nonblockConnect();
}

int BasicConnection::p_nonblockConnect()
{
  if ( m_socketId != -1 )
    return 0;

  m_errno = 0;
  m_lastConnectTime = time(NULL);
  int type = ( m_connKey.getTransport() == TRANSP_UDP ) ? SOCK_DGRAM: SOCK_STREAM;
  int fd = -1;
  int result = -1;
  char errstr[256];
  bool isV6=false;

  try
  {
    std::string ipStr= m_connKey.getServerIP();
    if(strchr(ipStr.c_str(),':')==0)
    {
      fd = socket(AF_INET, type, 0);
    }
    else
    {
      fd = socket(AF_INET6, type, 0);
      isV6=true;
    }

    if ( fd < 0 )
      throw std::string("Failed to create socket on") + toString();

    if ( fcntl(fd, F_SETFD, FD_CLOEXEC) != 0 )
      throw std::string("Failed to set socket option FD_CLOEXEC");

    if ( m_connKey.getTransport() == TRANSP_TCP )
    {
      /*
        int maxseg = 1460; // 1500 - 20 (ip hdr) - 20 (tcp hdr)
        if ( 0 == result && setsockopt(fd, SOL_TCP, TCP_MAXSEG, &maxseg, sizeof(maxseg)) != 0)
        result = -1;

        int val = 1;
        if ( 0 == result && setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, sizeof(val)) != 0)
        result = -1;
      */

      // Fix for "Address already in use" error
      //int optval = 1;
      //setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

      int maxSendBufferSize = (512L * 1024L);
      if ( setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &maxSendBufferSize, sizeof(maxSendBufferSize)) != 0 )
      {
        throw stringf("Failed to set TCP SND BUF SIZE reason=%d (%s)", errno, strerror_r(errno, errstr, sizeof(errstr)));
      }
      else
      {
        syslog(LOG_DEBUG, "set TCP SND BUF SIZE\n");
      }

      int maxRecvBufferSize = (512L * 1024L);
      if ( setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &maxRecvBufferSize, sizeof(maxRecvBufferSize)) != 0 )
      {
        throw stringf("Failed to set TCP RCV BUF SIZE. Reason=%d (%s)", errno, strerror_r(errno, errstr, sizeof(errstr)));
      }
      else
      {
        syslog(LOG_DEBUG, "set TCP RCV BUF SIZE\n");
      }
    }

    // set socket to non-block
    int flags = fcntl(fd, F_GETFL);
    if ( fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0 )
      throw std::string("Failed to set socket option O_NONBLOCK");

    if ( m_reservedPort  && !isV6)
    {
      struct sockaddr_in sa;
      sa.sin_family = AF_INET;
      sa.sin_addr.s_addr = 0;
      sa.sin_port = 0;

      int outcome = bindresvport(fd, &sa);
      if ( 0 != outcome )
        throw stringf("Failed to bind to reserved port. Reason=%d (%s)", errno, strerror_r(errno, errstr, sizeof(errstr)));
    }

    int outcome =-1;
    if(isV6) //ipv6 address
    {
      struct sockaddr_in6 serverAddr;
      serverAddr.sin6_family = AF_INET6;
      serverAddr.sin6_port = htons(m_connKey.getServerPort());
      inet_pton(AF_INET6,m_connKey.getServerIP().c_str(), &(serverAddr.sin6_addr));
      outcome = ::connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr) );
    }
    else
    {
      syslog(LOG_DEBUG, "Trying connecting to [%s] with fd=%d\n", m_connKey.getServerIP().c_str(),fd);
      // connect the socket
      struct sockaddr_in serverAddr;
      serverAddr.sin_family = PF_INET;
      serverAddr.sin_port = htons(m_connKey.getServerPort());
      inet_pton(AF_INET, m_connKey.getServerIP().c_str(), &(serverAddr.sin_addr));
      outcome = ::connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr) );
    }
    if ( outcome == 0 )
    {
      // immediate susccess
      m_socketId = fd;
      m_connectState = STATE_CONNECTED;
      syslog(LOG_DEBUG, "BasicConnection::%s: instant connect success\n", __func__);
      result = 0;
    }
    else
    {
      if ( errno != EINPROGRESS )
        throw stringf("Failed to connect fd(%d). Reason=%d (%s)", fd, errno, strerror_r(errno, errstr, sizeof(errstr)));

      m_socketId = fd;
      m_connectState = STATE_CONNECTING;
      result = 1;
    }
  }
  catch (const std::string& csErr)
  {
    syslog(LOG_ERR, "BasicConnection::%s: Server %s, %s\n", __func__, toString().c_str(), csErr.c_str());
  }
  catch (...)
  {
    syslog(LOG_ERR, "BasicConnection::%s: Server %s, %s\n", __func__, toString().c_str(), "An unhandled exception occurred");
  }

  if ( result == -1 && fd != -1 )
    ::close(fd);

  return result;
}


int BasicConnection::disconnect()
{
  MutexGuard lock(m_connMutexLock);
  return p_disconnect();
}

int BasicConnection::p_disconnect()
{
  int skt = m_socketId;
  if ( skt == -1 )
  {
    syslog(LOG_DEBUG, "BasicConnection::disconnect connection already disconnected %s:%d\n",
           getConnKey().getServerIP().c_str(), getConnKey().getServerPort());
    return 0;
  }

  if ( skt > 0 )
  {
    //::shutdown(skt, SHUTDOW_WR);
    ::close(skt);
  }

  m_connectState = STATE_RESET;
  m_socketId = -1;
  clearPendingWriteQueue();
  m_readState.reset();
  syslog(LOG_DEBUG, "BasicConnection::disconnect() called on skt=%d\n", skt);
  return 0;
}

int BasicConnection::ensureConnection(bool* pbReconnected)
{
  ///////////////////////////////////////////////////////////////////////////////////
  //
  // DO NOT USE THIS FUNCTION. THERE ARE BUGS IN IT. NO INTENTION OF FIXING IT NOW.
  //
  ///////////////////////////////////////////////////////////////////////////////////
  MutexGuard lock(m_connMutexLock);

  if ( pbReconnected ) *pbReconnected = false;

  // if the connection was not disconnected and if it is alive, return success
  if ( m_errno != ECONNRESET && p_isAlive() )
    return 0;

  syslog(LOG_DEBUG, "BasicConnection::%s: Force recreating RPC connections as connection to %s is DEAD.\n",
         __func__, toString().c_str());

  // Reconnect to the server
  // NOTE: Reconnection is mostly applicable for port mapper service.
  //       For other types the ports may change.
  //       It is advisable to call reconnect only for port mapper service
  if ( pbReconnected ) *pbReconnected = true;
  p_disconnect();
  int retVal = p_nonblockConnect();
  if ( retVal >= 0 )
  {
    if ( !p_isAlive() )
    {
      retVal =  -1;
      syslog(LOG_DEBUG, "BasicConnection::%s: Reconnected to server %s, but the connection is DEAD\n", __func__, toString().c_str());
    }
    else
    {
      syslog(LOG_DEBUG, "BasicConnection::%s: Reconnected to server %s\n", __func__, toString().c_str());
    }
  }
  else
  {
    syslog(LOG_DEBUG, "BasicConnection::%s: Cannot to server %s, retVal=%d\n", __func__, toString().c_str(), retVal);
  }

  return retVal;
}

int BasicConnection::recvNbyte()
{
  //MutexGuard lock(m_connMutexLock);
  int skt = getSocketId();
  if ( skt == -1 )
  {
    syslog(LOG_ERR, "BasicConnection::recvNbyte Invalid socket id for %s\n", toString().c_str());
    m_readState.reset();
    return -1;
  }

  do
  {
    int nread = recv(skt, m_readState.getBufferEnd(), m_readState.getByteToRead(), MSG_NOSIGNAL);
    if ( nread < 0 )
    {
      if ( errno == EAGAIN )
        return 1;
      if ( errno == EINTR )
        continue;

      char errstr[128];
      syslog(LOG_ERR, "BasicConnection::recvNByte(%s) got error=%d (%s)\n",
             toString().c_str(), errno, strerror_r(errno, errstr, sizeof(errstr)));
      break;
    }
    else if ( nread == 0 )
    {
      syslog(LOG_DEBUG, "BasicConnection::recvNByte connection %s is closed\n", toString().c_str());
	  errno = ECONNRESET;
      break;
    }
    else
    {
      m_readState.decrementByteToRead(nread);
      m_readState.incrementOffset(nread);

      if ( 0 == m_readState.getByteToRead() ) // complete message
      {
        m_readState.reset();
        return 0;
      }
    }
  }
  while (true);

  m_readState.reset();
  return -1;
}


void BasicConnection::addPendingWrite(RpcPacketPtr pkt)
{
  bool bSendNotify = false;
  {
    MutexGuard lock(m_pendingWriteLock);
    m_pendingWriteQueue.push(pkt);

    // first element in the queue
    if ( m_pendingWriteQueue.size() == 1 )
      bSendNotify = true;
  }
  if ( bSendNotify )
    ConnectionMgr::getInstance()->writeConnection(this);
}

RpcPacketPtr BasicConnection::getPendingWrite()
{
  RpcPacketPtr pkt = NULL;
  MutexGuard lock(m_pendingWriteLock);
  if ( !m_pendingWriteQueue.empty() )
  {
    pkt = m_pendingWriteQueue.front();
    //m_pendingWriteQueue.pop_front();
  }
  return pkt;
}

void BasicConnection::removePendingWrite()
{
  MutexGuard lock(m_pendingWriteLock);
  m_pendingWriteQueue.pop();
}

void BasicConnection::clearPendingWriteQueue()
{
  MutexGuard lock(m_pendingWriteLock);
  while ( !m_pendingWriteQueue.empty() )
    m_pendingWriteQueue.pop();
}

int BasicConnection::writePacket(RpcPacketPtr pkt)
{
  if ( pkt == NULL )
    return -1;

  int skt = getSocketId();
  if (skt == -1)
  {
    if (nonblockConnect() < 0)
    {
      syslog(LOG_ERR, "BasicConnection::sendPacket failed to connect to server %s:%d\n", toString().c_str(), getConnKey().getServerPort());
      return -1;
    }
  }

  pkt->resetWriteIndex();
  addPendingWrite(pkt);
  return 0;
}

// used by connection manager to send queued packets
int BasicConnection::sendPacket()
{
  int skt = getSocketId();
  if ( skt == -1 )
  {
    clearPendingWriteQueue();
    return -INVALID_SOCKET;
  }

  if ( getConnectionState() == STATE_CONNECTING )
    return SOCKET_BUSY;

  RpcPacketPtr pkt = getPendingWrite();
  while ( pkt != NULL )
  {
    syslog(LOG_DEBUG, "sendpacket to send xid=%d\n", pkt->getXid());
    int nwrite = send(skt, pkt->getWriteAddress(), pkt->getWriteSize(), MSG_NOSIGNAL);
    if ( nwrite <= 0 )
    {
      if ( errno != EAGAIN )
      {
        char errstr[128];
        syslog(LOG_DEBUG, "BasicConnection::sendPacket() server %s failed to send: %d (%s)\n",
               toString().c_str(), errno, strerror_r(errno, errstr, sizeof(errstr)) );

        clearPendingWriteQueue();
        return -SEND_FAIL;
      }
      return SOCKET_BUSY;
    }

    pkt->advanceWriteIndex(nwrite);

    if ( pkt->isWriteComplete() )
    {
      removePendingWrite();
      pkt = getPendingWrite();
    }
  }

  return SUCCESS;
}

bool BasicConnection::isAlive()
{
  MutexGuard lock(m_connMutexLock);
  return p_isAlive();
}

bool BasicConnection::p_isAlive()
{
  // This is a non-blocking socket, so we can call peek to see whether the socket is alive or not
  int error = 0;
  socklen_t len = sizeof(error);
  int retVal = getsockopt (m_socketId, SOL_SOCKET, SO_ERROR, &error, &len);
  return ( retVal == 0 && error == 0 );
}

bool BasicConnection::isConnected()
{
  MutexGuard lock(m_connMutexLock);
  return STATE_CONNECTED == m_connectState;
}

void BasicConnection::setInConnMgr(bool isInConnMgr)
{
  MutexGuard lock(m_connMutexLock);
  m_inConnMgr = isInConnMgr;
}

bool BasicConnection::isInConnMgr()
{
  MutexGuard lock(m_connMutexLock);
  return true == m_inConnMgr;
}

int BasicConnection::getSocketId() const
{
  MutexGuard lock(m_connMutexLock);
  return m_socketId;
}

void BasicConnection::setSocketId(int skt)
{
  MutexGuard lock(m_connMutexLock);
  m_socketId = skt;
}

void BasicConnection::setConnectionState(ConnectionState newstate)
{
  MutexGuard lock(m_connMutexLock);
  m_connectState = newstate;
}

ConnectionState BasicConnection::getConnectionState()
{
  MutexGuard lock(m_connMutexLock);
  return m_connectState;
}

int BasicConnection::getErrno() const
{
  MutexGuard lock(m_connMutexLock);
  return m_errno;
}

void BasicConnection::setErrno(int value)
{
  MutexGuard lock(m_connMutexLock);
  m_errno = value;
}

int BasicConnection::closeConnection()
{
  return disconnect();
}

} /* namespace OpenNfsC */

