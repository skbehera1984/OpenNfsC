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

#ifndef _MOUNT_CALL_
#define _MOUNT_CALL_

#include "RpcCall.h"
#include <nfsrpc/mount.h>

namespace OpenNfsC {

class Buffer;

namespace Mount {

class NullCall: public RemoteCall
{
  public:
    NullCall():RemoteCall(MOUNT, MOUNT_V3_NULL) {};
    ~NullCall() {};
  private:
    virtual int encodeArguments() { return 0; }
    virtual int decodeResults() { return 0; }
};

class MntCall : public RemoteCall
{
  public:
    MntCall(const dirpath&);
    ~MntCall();

    mountres3& getResult() { return res; }
  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    dirpath args;
    mountres3 res;
};

class UMountCall : public RemoteCall
{
  public:
    UMountCall(const dirpath&);
    ~UMountCall() {};

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    dirpath args;
};

class DumpCall : public RemoteCall
{
  public:
    DumpCall():RemoteCall(MOUNT, MOUNT_V3_DUMP),res(NULL),m_decodedStringBuffer(NULL) {}
    ~DumpCall();

    mountlist& getResult() { return res; }
  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    mountlist res;
    Buffer* m_decodedStringBuffer;
};

class UMntAllCall : public RemoteCall
{
  public:
    UMntAllCall():RemoteCall(MOUNT, MOUNT_V3_UMNTALL){};
    ~UMntAllCall() {};

  private:
    virtual int encodeArguments() { return 0; }
    virtual int decodeResults() { return 0; }
};

class ExportCall : public RemoteCall
{
  public:
    ExportCall();
    ~ExportCall();

    exports& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    exports res;
    groups m_groupBuffer;
    Buffer* m_decodedStringBuffer;
};

}} // end of namespace
#endif /* _MOUNT_CALL_ */
