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

#ifndef _NFS4_CALL_
#define _NFS4_CALL_

#include "RpcCall.h"
#include <nfsrpc/nfs4.h>

#define DEF_SMART_PTR(CLASS) typedef SmartPtr< CLASS > CLASS##Ptr

namespace OpenNfsC {
class Buffer;
namespace NFSv4 {


class NullCall: public RemoteCall
{
  public:
    NullCall():RemoteCall(NFS, NFSPROC4_NULL) {};
    ~NullCall() {};
  private:
    virtual int encodeArguments() { return 0; }
    virtual int decodeResults() { return 0; }
};
DEF_SMART_PTR(NullCall);

class COMPOUNDCall : public RemoteCall
{
  public:
    COMPOUNDCall();
    COMPOUNDCall(const COMPOUND4args&);
    ~COMPOUNDCall();
    COMPOUND4res& getResult() { return res; }

    int appendCommand(const nfs_argop4 *cmd);
    void clear();
    void clearArgs();
    void clearRes();

    int findOPIndex(int op);

  private:
    void freeArg(nfs_argop4 *arg);
    void freeRes(nfs_resop4 *res);

  public:
    int encode_OP_ACCESS(RpcPacketPtr packet, const ACCESS4args *arg);
    int decode_OP_ACCESS(RpcPacketPtr packet, ACCESS4res *res);
    int encode_OP_CLOSE(RpcPacketPtr packet, const CLOSE4args *arg);
    int decode_OP_CLOSE(RpcPacketPtr packet, CLOSE4res *res);
    int encode_OP_COMMIT(RpcPacketPtr packet, const COMMIT4args *arg);
    int decode_OP_COMMIT(RpcPacketPtr packet, COMMIT4res *res);
    int encode_OP_CREATE(RpcPacketPtr packet, const CREATE4args *arg);
    int decode_OP_CREATE(RpcPacketPtr packet, CREATE4res *arg);
    int encode_OP_DELEGPURGE(RpcPacketPtr packet, const DELEGPURGE4args *arg);
    int decode_OP_DELEGPURGE(RpcPacketPtr packet, DELEGPURGE4res *arg);
    int encode_OP_DELEGRETURN(RpcPacketPtr packet, const DELEGRETURN4args *arg);
    int decode_OP_DELEGRETURN(RpcPacketPtr packet, DELEGRETURN4res *arg);
    int encode_OP_GETATTR(RpcPacketPtr packet, const GETATTR4args *arg);
    int decode_OP_GETATTR(RpcPacketPtr packet, GETATTR4res *arg);
    int encode_OP_GETFH(RpcPacketPtr packet);
    int decode_OP_GETFH(RpcPacketPtr packet, GETFH4res *res);
    int encode_OP_LINK(RpcPacketPtr packet, const LINK4args *arg);
    int decode_OP_LINK(RpcPacketPtr packet, LINK4res *res);
    int encode_OP_LOCK(RpcPacketPtr packet, const LOCK4args *arg);
    int decode_OP_LOCK(RpcPacketPtr packet, LOCK4res *res);
    int encode_OP_LOCKT(RpcPacketPtr packet, const LOCKT4args *arg);
    int decode_OP_LOCKT(RpcPacketPtr packet, LOCKT4res *res);
    int encode_OP_LOCKU(RpcPacketPtr packet, const LOCKU4args *arg);
    int decode_OP_LOCKU(RpcPacketPtr packet, LOCKU4res *res);
    int encode_OP_LOOKUP(RpcPacketPtr packet, const LOOKUP4args *arg);
    int decode_OP_LOOKUP(RpcPacketPtr packet, LOOKUP4res *res);
    int encode_OP_LOOKUPP(RpcPacketPtr packet);
    int decode_OP_LOOKUPP(RpcPacketPtr packet, LOOKUPP4res *res);
    int encode_OP_NVERIFY(RpcPacketPtr packet, const NVERIFY4args *arg);
    int decode_OP_NVERIFY(RpcPacketPtr packet, NVERIFY4res *res);
    int encode_OP_OPEN(RpcPacketPtr packet, const OPEN4args *arg);
    int decode_OP_OPEN(RpcPacketPtr packet, OPEN4res *res);
    int encode_OP_OPENATTR(RpcPacketPtr packet, const OPENATTR4args *arg);
    int decode_OP_OPENATTR(RpcPacketPtr packet, OPENATTR4res *res);
    int encode_OP_OPEN_CONFIRM(RpcPacketPtr packet, const OPEN_CONFIRM4args *arg);
    int decode_OP_OPEN_CONFIRM(RpcPacketPtr packet, OPEN_CONFIRM4res *res);
    int encode_OP_OPEN_DOWNGRADE(RpcPacketPtr packet, const OPEN_DOWNGRADE4args *arg);
    int decode_OP_OPEN_DOWNGRADE(RpcPacketPtr packet, OPEN_DOWNGRADE4res *res);
    int encode_OP_PUTFH(RpcPacketPtr packet, const PUTFH4args *arg);
    int decode_OP_PUTFH(RpcPacketPtr packet, PUTFH4res *res);
    int encode_OP_PUTPUBFH(RpcPacketPtr packet);
    int decode_OP_PUTPUBFH(RpcPacketPtr packet, PUTPUBFH4res *res);
    int encode_OP_PUTROOTFH(RpcPacketPtr packet);
    int decode_OP_PUTROOTFH(RpcPacketPtr packet, PUTROOTFH4res *res);
    int encode_OP_READ(RpcPacketPtr packet, const READ4args *arg);
    int decode_OP_READ(RpcPacketPtr packet, READ4res *res);
    int encode_OP_READDIR(RpcPacketPtr packet, const READDIR4args *arg);
    int decode_OP_READDIR(RpcPacketPtr packet, READDIR4res *res);
    int encode_OP_READLINK(RpcPacketPtr packet);
    int decode_OP_READLINK(RpcPacketPtr packet, READLINK4res *res);
    int encode_OP_REMOVE(RpcPacketPtr packet, const REMOVE4args *arg);
    int decode_OP_REMOVE(RpcPacketPtr packet, REMOVE4res *res);
    int encode_OP_RENAME(RpcPacketPtr packet, const RENAME4args *arg);
    int decode_OP_RENAME(RpcPacketPtr packet, RENAME4res *res);
    int encode_OP_RENEW(RpcPacketPtr packet, const RENEW4args *arg);
    int decode_OP_RENEW(RpcPacketPtr packet, RENEW4res *res);
    int encode_OP_RESTOREFH(RpcPacketPtr packet);
    int decode_OP_RESTOREFH(RpcPacketPtr packet, RESTOREFH4res *res);
    int encode_OP_SAVEFH(RpcPacketPtr packet);
    int decode_OP_SAVEFH(RpcPacketPtr packet, SAVEFH4res *res);
    int encode_OP_SECINFO(RpcPacketPtr packet);
    int decode_OP_SECINFO(RpcPacketPtr packet);
    int encode_OP_SETATTR(RpcPacketPtr packet, const SETATTR4args *arg);
    int decode_OP_SETATTR(RpcPacketPtr packet, SETATTR4res *res);
    int encode_OP_SETCLIENTID(RpcPacketPtr packet, SETCLIENTID4args *arg);
    int decode_OP_SETCLIENTID(RpcPacketPtr packet, SETCLIENTID4res *res);
    int encode_OP_SETCLIENTID_CONFIRM(RpcPacketPtr packet, SETCLIENTID_CONFIRM4args *arg);
    int decode_OP_SETCLIENTID_CONFIRM(RpcPacketPtr packet, SETCLIENTID_CONFIRM4res *res);
    int encode_OP_VERIFY(RpcPacketPtr packet, const VERIFY4args *arg);
    int decode_OP_VERIFY(RpcPacketPtr packet, VERIFY4res *res);
    int encode_OP_WRITE(RpcPacketPtr packet, const WRITE4args *arg);
    int decode_OP_WRITE(RpcPacketPtr packet, WRITE4res *res);
    int encode_OP_RELEASE_LOCKOWNER(RpcPacketPtr packet, const RELEASE_LOCKOWNER4args *arg);
    int decode_OP_RELEASE_LOCKOWNER(RpcPacketPtr packet, RELEASE_LOCKOWNER4res *res);

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    COMPOUND4args args;
    COMPOUND4res  res;
};
DEF_SMART_PTR(COMPOUNDCall);


}} // end of namespace
#endif /* _NFS4_CALL_ */
