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

#include "ConnectionMgr.h"
#include "ConnectionMgr.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <errno.h>
#include <syslog.h>

using namespace std;

namespace {  //unamed namesapce

// set socket to nonblock or block
inline bool setNonblock(int skt, bool nonblock)
{
  int flags;
  flags = fcntl(skt, F_GETFL);
  if ( nonblock )
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  int res = fcntl(skt, F_SETFL, flags);
  return (res != -1);
}

inline int getSocketError(int skt, int& error)
{
  socklen_t len = sizeof(error);
  return ( getsockopt(skt, SOL_SOCKET, SO_ERROR, &error, &len) != 0)? -1 : 0;
}

} // end of unamed namespace

using OpenNfsC::Thread::MutexGuard;

namespace OpenNfsC {

ConnectionMgr * ConnectionMgr::m_connMgrPtr = nullptr;
Thread::Mutex ConnectionMgr::m_connMgrLock(false);
const int epollEventSize = 128;

ConnectionMgr * ConnectionMgr::getInstance()
{
  MutexGuard lock(m_connMgrLock);
  if ( m_connMgrPtr == nullptr )
  {
    m_connMgrPtr = new ConnectionMgr();
    if ( m_connMgrPtr == nullptr )
      throw std::string("Failed to create Connection Manager object");
  }
  return m_connMgrPtr;
}

ConnectionMgr::ConnectionMgr():m_sktMap(this),m_epollfd(-1),m_msgListLock(false)
{
  if ( enable() )
  {
    start();
    syslog(LOG_DEBUG, "Connection Manager is started successfully\n");
  }
}

ConnectionMgr::~ConnectionMgr()
{
  if ( isRunning() )
  {
    stop();
    sendControlMsg(NULL, STOP_MGR);
    join();
    syslog(LOG_DEBUG, "Connection Manager is stopped successfully\n");
  }
  disable();
}

bool ConnectionMgr::enable()
{
  int res = epoll_create(epollEventSize);
  if ( res < 0 )
  {
    char errstr[256];
    syslog(LOG_ERR, "epoll_create failed due to error %s\n", strerror_r(errno, errstr, sizeof(errstr)));
    return false;
  }
  m_epollfd = res;

  socketpair(AF_UNIX, SOCK_STREAM, 0, m_controlfds);
  setNonblock(m_controlfds[0], true);
  setNonblock(m_controlfds[1], true);

  syslog(LOG_DEBUG, "ConnectionMgr: add control socketpair (%d,%d)\n", m_controlfds[0], m_controlfds[1]);

  return true;
}

bool ConnectionMgr::disable()
{
  syslog(LOG_DEBUG, "ConnectionMgr::disable()\n");

  if (close(m_controlfds[0]) < 0)
    cout << "ConnectionMgr::disable() failed to close m_controlfds[0]" << endl;
  m_controlfds[0] = -1;

  if (close(m_controlfds[1]) < 0)
    cout << "ConnectionMgr::disable() failed to close m_controlfds[1]" << endl;
  m_controlfds[1] = -1;

  if (close(m_epollfd) < 0)
    cout << "ConnectionMgr::disable() failed to close m_epollfd" << endl;
  m_epollfd = -1;

  return true;
}


void ConnectionMgr::processControlMsg()
{
  char zeroes[256];

  // read as much data from control socket as possible
  while (true)
  {
    int nread = recv(m_controlfds[0], zeroes, sizeof(zeroes), MSG_NOSIGNAL);
    if (nread < 0)
    {
      if (errno == EAGAIN)
        break;
      if (errno == EINTR)
        continue;
      char errstr[256];

      syslog(LOG_ERR, "processControlMsg failed due to error %s\n", strerror_r(errno, errstr, sizeof(errstr)));
      return;
    }
    else if (nread == 0)
    {
      syslog(LOG_ERR, "ConnectionMgr::processControlMsg() control socket has been shutdown\n");
      return;
    }
  }

  // process control messages
  ControlMessage msg;

  do
  {
    {
      MutexGuard lock(m_msgListLock);
      if ( m_controlMsgList.empty() )
        break;
      msg = m_controlMsgList.front();
      m_controlMsgList.pop_front();
    }

    if ( msg.second == NULL )
      continue;

    if ( msg.first == ADD_SKT )
    {
      //int res = msg.second->connect();
      //if (res == 0)
      syslog(LOG_DEBUG, "ConnectionMgr::processControlMsg() add socket %d\n", msg.second->getSocketId());
      m_sktMap.add(msg.second);
    }
    else if ( msg.first == DEL_SKT )
    {
      //if ( msg.second->getSocketId() > 0 )
      {
        syslog(LOG_DEBUG, "ConnectionMgr::processControlMsg() delete socket %d\n", msg.second->getSocketId());
        m_sktMap.remove(msg.second);
        msg.second->closeConnection();
      }
      msg.second = NULL;
    }
    else if ( msg.first == WRITE_SKT )
    {
      syslog(LOG_DEBUG, "ConnectionMgr::processControlMsg() write socket %d\n", msg.second->getSocketId());
      int outcome = msg.second->sendPacket();
      if (outcome == 1)
        epollAdd(msg.second->getSocketId(), EPOLLIN|EPOLLOUT, true);
      else if (outcome < 0)
      {
        syslog(LOG_ERR, "ConnectionMgr::processControlMsg() write socket failed to send packet\n");
        m_sktMap.remove(msg.second);
        msg.second->closeConnection();
        msg.second = NULL;
      }
    }
  } while(true);
}

bool ConnectionMgr::sendControlMsg(const BasicConnectionPtr& conn, enum Action op)
{
  if (!isRunning())
  {
    syslog(LOG_ERR, "ConnectionMgr::sendControlMsg %d connectionMgr thread is not running\n", op);
    return false;
  }

  {
    // add control message to control message list
    MutexGuard lock(m_msgListLock);
    m_controlMsgList.push_back(std::make_pair(op, conn));
  }

  const uint8 zero = 0;
  // send one byte to control socket to wake up epoll
  if (send(m_controlfds[1], &zero, sizeof(zero), MSG_NOSIGNAL) < 0)
  {
    syslog(LOG_ERR, "ConnectionMgr::sendControlMsg failed to send\n");
    return false;
  }

  return true;
}

bool ConnectionMgr::startMgr()
{
  ConnectionMgr* mgr = getInstance();
  if ( !mgr )
  {
    syslog(LOG_WARNING, "failed to restart Connection Manager\n");
    return false;
  }

  // start connection manager thread
  syslog(LOG_DEBUG, "Connection Manager is started successfully\n");
  return true;
}

void ConnectionMgr::stopMgr()
{
  if (m_connMgrPtr && m_connMgrPtr->isRunning())
  {
    m_connMgrPtr->stop();
    m_connMgrPtr->sendControlMsg(NULL, STOP_MGR);
    m_connMgrPtr->join();
    m_connMgrPtr->disable();
    syslog(LOG_DEBUG, "Connection Manager is stopped successfully\n");
  }
}

bool ConnectionMgr::epollAdd(int skt, uint32 events, bool update)
{
  int op = EPOLL_CTL_ADD;
  struct epoll_event event;
  memset(&event, 0, sizeof(event) );
  event.data.fd = skt;
  event.events = events;

  if ( update )
    op = EPOLL_CTL_MOD;

  if ( epoll_ctl(m_epollfd, op, skt, &event) != 0 )
  {
    syslog(LOG_ERR, "ConnectionMgr::epollAdd failed to add socket\n");
    return false;
  }

  return true;
}

bool ConnectionMgr::epollDel(int skt)
{
  if ( fcntl(skt, F_GETFL) < 0 && errno == EBADF )
    return false;

  int op = EPOLL_CTL_DEL;
  struct epoll_event event;
  memset(&event, 0, sizeof(event) );
  event.data.fd = skt;

  if ( epoll_ctl(m_epollfd, op, skt, &event) != 0 )
  {
    char errstr[256];
    if ( errno == EBADF || errno == ENOENT || errno == EPERM )
    {
      syslog(LOG_DEBUG, "ConnectionMgr::epollDel failed to delete skt=%d due to %s\n",
             skt, strerror_r(errno, errstr, sizeof(errstr)));
    }
    else
    {
      syslog(LOG_CRIT, "ConnectionMgr::epollDel failed to delete skt=%d due to %s\n",
             skt, strerror_r(errno, errstr, sizeof(errstr)));
      //abort();
    }

    syslog(LOG_ERR, "ConnectionMgr::epollDel failed to remove socket %d\n", skt);
    return false;
  }

  return true;
}

bool ConnectionMgr::addConnection(const BasicConnectionPtr& conn)
{
  return sendControlMsg(conn, ADD_SKT);
}

bool ConnectionMgr::removeConnection(const BasicConnectionPtr& conn)
{
  return sendControlMsg(conn, DEL_SKT);
}

bool ConnectionMgr::writeConnection(BasicConnectionPtr bconn)
{
  BasicConnectionPtr conn = m_sktMap.get(bconn);
  if ( conn.empty() )
    return false;
  return sendControlMsg(conn, WRITE_SKT);
}

void ConnectionMgr::processProtocolMsg(int skt, const epoll_event& event)
{
  BasicConnectionPtr conn = NULL;

  try
  {
    conn = m_sktMap.get(skt);
    if ( conn == NULL )
    {
      syslog(LOG_WARNING, "ConnectionMgr::%s: got unknown skt %d\n", __func__, skt);
      // Remove the socket from polling
      epollDel(skt);
      return;
    }
    if ( skt != conn->getSocketId() )
    {
      syslog(LOG_DEBUG, "ConnectionMgr::%s: socket mismatch [%d:%d]. Removing connection.\n", __func__, skt, conn->getSocketId());
      throw -1;
    }

    if ( event.events & (EPOLLERR | EPOLLHUP) )
    {
      syslog(LOG_DEBUG, "ConnectionMgr::%s: socket %d got %s event from %s. conn: %p\n",
             __func__, skt, (event.events & EPOLLERR)?"EPOLLERR":"EPOLLHUP", conn->toString().c_str(), conn.ptr());
      conn->recvPacket();
      throw -1;
    }

    if ( event.events & EPOLLIN )
    {
      syslog(LOG_DEBUG, "ConnectionMgr::%s: socket %d got EPOLLIN event from %s\n",
             __func__, skt, conn->toString().c_str());
      int outcome = 0;
      do
      {
        outcome = conn->recvPacket();
      } while(outcome == 0);

      if (outcome < 0)
      {
        char tErrStr[ 128 ];
        conn->setErrno(errno);

        syslog(LOG_WARNING, "ConnectionMgr::%s: recvPacket() from %s skt=%d outcome=%d errno=%d (%s)\n",
               __func__, conn->toString().c_str(), skt, outcome, errno,
               strerror_r(errno, tErrStr, sizeof(tErrStr)));
        throw -1;
      }
      conn->setErrno(0);
    }

    if ( event.events & EPOLLOUT )
    {
      if (conn->getConnectionState() == STATE_CONNECTING)
      {
        syslog(LOG_DEBUG, "ConnectionMgr::%s: got connection event from %s\n",
               __func__, conn->toString().c_str());
        int error = 0;
        if (getSocketError(skt, error) < 0 || error != 0)
        {
          syslog(LOG_ERR, "ConnectionMgr::%s: socket %d fail to connect to %s\n", __func__, skt, conn->toString().c_str());
          throw -1;
        }

        syslog(LOG_DEBUG, "ConnectionMgr::%s: socket %d connected to %s\n", __func__, skt, conn->toString().c_str());
        conn->setConnectionState(STATE_CONNECTED);
      }

      int outcome = conn->sendPacket();
      if (outcome < 0)
      {
        syslog(LOG_ERR, "ConnectionMgr::%s: sendPacket %d encountered error to %s\n", __func__, skt, conn->toString().c_str());
        throw -1;
      }
      if (outcome == 0) // no more write
        epollAdd(skt, EPOLLIN, true);
    }
  }
  catch (int errCode)
  {
    if ( errCode )
    {
      m_sktMap.remove(skt);
      if ( conn )
      {
        conn->closeConnection();
        conn = NULL;
      }
    }
  }
  catch (...)
  {
    syslog(LOG_ERR, "ConnectionMgr::%s: An unhandled exception occurred on socket %d\n", __func__, skt);
  }
}

void ConnectionMgr::run()
{
  epollAdd(m_controlfds[0], EPOLLIN);

  while ( continueRunning() )
  {
    try
    {
      struct epoll_event events[epollEventSize];
      int nfds = epoll_wait(m_epollfd, events, epollEventSize, -1);

      for ( int i = 0; i < nfds; i++ )
      {
        int skt = events[i].data.fd;

        if (skt == m_controlfds[0])
          processControlMsg();
        else
          processProtocolMsg(skt, events[i]);
      }
    }
    catch (const std::exception& e)
    {
      syslog(LOG_ERR, "ConnectionMgr::%s: %s\n", __func__, e.what());
    }
    catch (...)
    {
      syslog(LOG_ERR, "ConnectionMgr::%s: An unhandled exception occurred\n", __func__);
    }
  }

  if ( !m_sktMap.empty() )
  {
    syslog(LOG_DEBUG, "ConnectionMgr::%s: Deleting %zu connections from socket map\n", __func__, m_sktMap.size());
    m_sktMap.clear();
  }

  epollDel(m_controlfds[0]);
}

/////////////////////////////////////////////////////////////////////////////////////////
//
// Implementation of SocketConnectionMap
//
/////////////////////////////////////////////////////////////////////////////////////////
bool SocketConnectionMap::add(BasicConnectionPtr conn)
{
  if ( conn == NULL )
    return false;

  int skt = conn->getSocketId();
  {
    MutexGuard lock(m_mutex);
    SocketConnectionMap::iterator it = this->find(skt);
    if ( it != this->end() )
    {
      BasicConnectionPtr oldConn = it->second;
      if ( oldConn == conn ) // already in map
        return true;

      syslog(LOG_DEBUG, "SocketConnectionMap::%s: Remove old connection(%p) socket %d\n", __func__, oldConn.ptr(), skt);
      it->second = conn;
    }
    else
    {
      this->insert(std::make_pair(skt, conn));
      conn->setInConnMgr(true);
    }
  }

  int events = EPOLLIN;
  if( !conn->isConnected() )
    events |= EPOLLOUT;
  if ( m_mgr ) m_mgr->epollAdd(skt, events);

  syslog(LOG_DEBUG, "SocketConnectionMap::%s: Added connection(%p) socket %d. Size: %zu\n",
         __func__, conn.ptr(), skt, this->size());

  return true;
}

bool SocketConnectionMap::remove(int skt)
{
  if (skt < 0) return false;

  syslog(LOG_DEBUG, "SocketConnectionMap::%s: remove socket %d\n", __func__, skt);
  {
    MutexGuard lock(m_mutex);
    SocketConnectionMap::iterator it = this->find(skt);
    if ( it != this->end() )
    {
      BasicConnectionPtr conn = it->second;
      this->erase(it);
      if ( conn != NULL )
        conn->setInConnMgr(false);

      syslog(LOG_DEBUG, "SocketConnectionMap::%s: removed connection(%p) socket %d. Size: %zu\n",
             __func__, conn.ptr(), skt, SocketConnectionMapBase::size());
    }
    else
    {
      syslog(LOG_DEBUG, "SocketConnectionMap::%s: socket %d not found for deletion. Size: %zu\n",
             __func__, skt, SocketConnectionMapBase::size());
    }

    if ( m_mgr ) m_mgr->epollDel(skt);
  }
  return true;
}

bool SocketConnectionMap::remove(const BasicConnectionPtr& conn)
{
  MutexGuard lock(m_mutex);
  for ( SocketConnectionMap::iterator it = this->begin(); it != this->end(); it++ )
  {
    if ( it->second == conn )
    {
      int skt = (conn == NULL)? -1 : conn->getSocketId();
      this->erase(it);
      if ( m_mgr ) m_mgr->epollDel(skt);
      syslog(LOG_DEBUG, "SocketConnectionMap::%s: removed connection(%p) socket %d. Size: %zu\n",
             __func__, conn.ptr(), skt, SocketConnectionMapBase::size());
      return true;
    }
  }

  return false;
}

BasicConnectionPtr SocketConnectionMap::get(int skt)
{
  MutexGuard lock(m_mutex);
  SocketConnectionMap::iterator it = this->find(skt);
  return ( it != this->end() )? it->second : NULL;
}

BasicConnectionPtr SocketConnectionMap::get(const BasicConnectionPtr& bconn)
{
  MutexGuard lock(m_mutex);
  BasicConnectionPtr conn;
  for ( SocketConnectionMap::iterator it = this->begin(); it != this->end(); ++it )
  {
    if ( it->second == bconn )
    {
      conn = it->second;
      break;
    }
  }
  return conn;
}

void SocketConnectionMap::clear()
{
  syslog(LOG_DEBUG, "SocketConnectionMap::%s\n", __func__);

  MutexGuard lock(m_mutex);
  std::map<int, BasicConnectionPtr>::iterator iter;
  for ( SocketConnectionMap::iterator it = this->begin(); it != this->end(); ++it )
  {
    syslog(LOG_DEBUG, "SocketConnectionMap::%s: cleared %d\n", __func__, it->first);
    it->second->setInConnMgr(false);
    if ( m_mgr ) m_mgr->epollDel(iter->first);
    it->second = NULL;
  }

  // clear all the entries from the map
  SocketConnectionMapBase::clear();
}

bool SocketConnectionMap::empty() const
{
  MutexGuard lock(m_mutex);
  return SocketConnectionMapBase::empty();
}

size_t SocketConnectionMap::size() const
{
  MutexGuard lock(m_mutex);
  return SocketConnectionMapBase::size();
}

} /* namespace OpenNfsC */

