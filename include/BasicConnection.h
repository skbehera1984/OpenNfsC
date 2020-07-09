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

#ifndef _BASICCONNECTION_H_
#define _BASICCONNECTION_H_

#include "RpcPacket.h"
#include "RpcDefs.h"
#include <string>
#include <SmartPtr.h>
#include <stdTypes.h>
#include <Thread.h>
#include <queue>

namespace OpenNfsC {

class RpcPacket;

enum ErrorCode
{
  SUCCESS = 0,
  SOCKET_BUSY,
  INVALID_SOCKET,
  SEND_FAIL,
  RECV_FAIL,
  IOCTL_FAIL,
  UNKNOWN_FAIL
};

enum ConnectionState
{
  STATE_UNKNOWN = 0,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_RESET
};

class BasicConnection;
typedef SmartPtr<BasicConnection> BasicConnectionPtr;

// ConnKey class definition
// it is essential a tuple: server IP, server port, transport type
class ConnKey
{
  private:
    ConnKey():m_serverIP(""),m_serverPort(0) {}
  public:
    ConnKey(std::string serverIP, uint16 serverPort, enum TransportType proto):
      m_serverIP(serverIP),m_serverPort(serverPort),m_transp(proto) {}
    ConnKey(ConnKey& rhs):
      m_serverIP(rhs.m_serverIP),m_serverPort(rhs.m_serverPort),m_transp(rhs.m_transp) {};

    uint16 getServerPort() const{ return m_serverPort; }
    std::string getServerIP() const {return m_serverIP; }
    enum TransportType getTransport() const { return m_transp; }
  private:
    std::string m_serverIP;
    uint16 m_serverPort;
    enum TransportType m_transp;
};


class BasicConnection : public SmartRef
{
  private:
    BasicConnection();
    BasicConnection(const BasicConnection&);
  public:
    BasicConnection(ConnKey& key, bool resv=false);
    virtual ~BasicConnection();

    // non-blocking connect
    // On success, will add connection to connection Manager
    int nonblockConnect();
    int p_nonblockConnect();

    // close socket
    // remove connection from connection manager if it is still in connection manager
    int disconnect();
    int p_disconnect();

    // Make sure the connection is available to send a request
    int ensureConnection(bool* pbReconnected = nullptr);

    virtual int sendAndWait(RpcPacketPtr request, RpcPacketPtr& reply, int timeout_ms) = 0;

    // add packet to pending write queue and notify connection manager to send
    virtual int writePacket(RpcPacketPtr);

    // used by connection manager to send/recv packet Must be non-blocking
    int sendPacket();
    virtual int recvPacket() = 0;

    // connection cleanup
    virtual int closeConnection();
    virtual RpcPacketPtr createRequest(int) = 0;

    int recvNbyte();

    const ConnKey& getConnKey() const { return m_connKey; }
    const char* getServerIpStr() const { return m_serverIPStr; }
    bool useReservedPort() const { return m_reservedPort; }
    virtual std::string toString() const = 0;

    // lock protected
    bool isAlive();
    bool p_isAlive();
    bool isConnected();
    void setInConnMgr(bool isInConnMgr);
    bool isInConnMgr();
    int getSocketId() const;
    void setSocketId(int skt);
    void setConnectionState(enum ConnectionState);
    enum ConnectionState getConnectionState();

    int getErrno() const;
    void setErrno(int value);

  protected:

    // class defination of read state
    class ReadState
    {
      private:
        ReadState(ReadState&); //not implemented
      public:
        ReadState():m_initialized(false), m_offset(0),m_byteToRead(0),m_buf(NULL) {}
        ReadState(void* buf, uint32 byteToRead):
          m_initialized(false), m_offset(0),m_byteToRead(byteToRead),m_buf(buf) {}
        void incrementOffset(uint32 nread) { m_offset += nread; }
        void decrementByteToRead(uint32 nread) { if (m_byteToRead >= nread) m_byteToRead -= nread; }
        void initialize(void* buf, uint32 byteToRead)
        {
          m_offset = 0;
          m_byteToRead = byteToRead;
          m_buf = buf;
          m_initialized = true;
        }
        void reset()
        {
          m_offset = 0;
          m_byteToRead = 0;
          m_buf = NULL;
          m_initialized = false;
        }
        bool isInitialized() { return m_initialized; }
        uint32 getByteToRead() { return m_byteToRead; }
        void* getBufferEnd() { return (char*)m_buf + m_offset; }
      private:
        // true if ReadState instance is initialized
        bool m_initialized;
        // currrent read offset
        uint32 m_offset;
        // remaining bytes to read
        uint32 m_byteToRead;
        // buffer to store data read
        void* m_buf;
    };
    ReadState m_readState;

  private:
    // functions to add/remove packets to pending write queue
    void addPendingWrite(RpcPacketPtr);
    void removePendingWrite();
    void clearPendingWriteQueue();
    RpcPacketPtr getPendingWrite();

  private:
    // Mutex to protect data members
    mutable OpenNfsC::Thread::Mutex m_connMutexLock;

    // Inet connection key
    ConnKey m_connKey;

    // flag to indicate whether to use reserved port or not. true means reserved port will be required
    bool m_reservedPort;

    // socket id.
    int m_socketId;

    // connection state
    enum ConnectionState m_connectState;

    // whether connection is added to connection manager
    bool m_inConnMgr;

    time_t m_lastConnectTime;

    // mutex lock to protect pending write queue
    OpenNfsC::Thread::Mutex m_pendingWriteLock;

    // pending write queue
    std::queue<RpcPacketPtr> m_pendingWriteQueue;

   // char m_serverIPStr[16];
   char m_serverIPStr[46];

   int m_errno;
};

} //end namespace
#endif /* _BASICCONNECTION_H_ */
