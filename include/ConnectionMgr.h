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

#ifndef _CONN_MGR_H_
#define _CONN_MGR_H_

/**************************************************
 * Singeton Connection Manager Class
 *************************************************/

#include "BasicConnection.h"

#include <list>
#include <map>
#include <utility>
#include <Thread.h>
#include <stdTypes.h>
#include <sys/epoll.h>

namespace OpenNfsC {

class ConnectionMgr;

typedef std::map<int, BasicConnectionPtr> SocketConnectionMapBase;
class SocketConnectionMap : protected SocketConnectionMapBase
{
public:
  SocketConnectionMap(ConnectionMgr* mgr) : m_mgr(mgr), m_mutex(false) {}
  virtual ~SocketConnectionMap() { clear(); }
  bool add(BasicConnectionPtr conn);
  bool remove(int skt);
  bool remove(const BasicConnectionPtr& conn);
  BasicConnectionPtr get(int skt);
  BasicConnectionPtr get(const BasicConnectionPtr& bconn);
  void clear();
  bool empty() const;
  size_t size() const;

private:
  SocketConnectionMap(); // not implemented
  ConnectionMgr* m_mgr;
  mutable OpenNfsC::Thread::Mutex m_mutex;
};

// Connection manager
class ConnectionMgr : public Thread::Thread
{
  friend class SocketConnectionMap;
  private:
    ConnectionMgr(const ConnectionMgr&); // not implemented
    ConnectionMgr& operator=(const ConnectionMgr&); // not implemented
    ~ConnectionMgr();

    // enable epoll and control socket
    bool enable();

    //disable epoll and control socket
    bool disable();

    // process control messages
    void processControlMsg();
    // process protocol messages
    void processProtocolMsg(int skt, const epoll_event& event);

    // add fd to epoll
    bool epollAdd(int, uint32, bool update = false);

    // remove fd from epoll
    bool epollDel(int);

    ConnectionMgr();

  public:
    static bool startMgr();
    static void stopMgr();
    static ConnectionMgr* getInstance();

    enum Action
    {
        ADD_SKT,
        DEL_SKT,
        WRITE_SKT,
        STOP_MGR,
        NOOP
    };

    typedef std::pair<enum Action, BasicConnectionPtr> ControlMessage;

    // send control message to connection manager to add a connection
    bool addConnection(const BasicConnectionPtr& conn);
    // send control message to connection manager to remove a connection
    bool removeConnection(const BasicConnectionPtr& conn);
    // send control message to connection manager to write packets to a connection
    bool writeConnection(BasicConnectionPtr bconn);

    virtual void run();

  private:
    bool sendControlMsg(const BasicConnectionPtr&, enum Action action);

  private:
    // map of connected connections, keyed by socket id
    // it is not lock protected since there is only one thread (i.e. connection manager thread) modifying it.
    SocketConnectionMap m_sktMap;

    // epoll fd
    int m_epollfd;
    // socket pair to send control messages to connection mananer thread
    int m_controlfds[2];

    // control messege list's lock
    OpenNfsC::Thread::Mutex m_msgListLock;

    // control messege list
    std::list<ControlMessage> m_controlMsgList;

    static OpenNfsC::Thread::Mutex m_connMgrLock;
    static ConnectionMgr * m_connMgrPtr;
};

} // end of namespace

#endif /* _CONN_MGR_H_ */
