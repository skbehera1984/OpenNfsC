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

#ifndef _NFS_CALL_
#define _NFS_CALL_

#include "RpcCall.h"
#include <nfsrpc/mount.h>

#define DEF_SMART_PTR(CLASS) typedef SmartPtr< CLASS > CLASS##Ptr

namespace OpenNfsC {
class Buffer;
namespace NFSv3 {

class NullCall: public RemoteCall
{
  public:
    NullCall():RemoteCall(NFS, NFS_V3_NULL) {};
    ~NullCall() {};
  private:
    virtual int encodeArguments() { return 0; }
    virtual int decodeResults() { return 0; }
};
DEF_SMART_PTR(NullCall);

class GetAttrCall: public RemoteCall
{
  public:
    GetAttrCall(const GETATTR3args&);
    ~GetAttrCall();
    GETATTR3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    GETATTR3args args;
    GETATTR3res res;
};
DEF_SMART_PTR(GetAttrCall);


class SetAttrCall: public RemoteCall
{
  public:
    SetAttrCall(const SETATTR3args&);
    ~SetAttrCall();
    SETATTR3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    SETATTR3args args;
    SETATTR3res res;
};
DEF_SMART_PTR(SetAttrCall);

class LookUpCall: public RemoteCall
{
  public:
    LookUpCall(const LOOKUP3args&);
    ~LookUpCall();
    LOOKUP3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    LOOKUP3args args;
    LOOKUP3res res;
};
DEF_SMART_PTR(LookUpCall);

class AccessCall: public RemoteCall
{
  public:
    AccessCall(const ACCESS3args&);
    ~AccessCall();
    ACCESS3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    ACCESS3args args;
    ACCESS3res res;
};
DEF_SMART_PTR(AccessCall);

class ReadLinkCall: public RemoteCall
{
  public:
    ReadLinkCall(const READLINK3args&);
    ~ReadLinkCall();
    READLINK3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    READLINK3args args;
    READLINK3res res;
};
DEF_SMART_PTR(ReadLinkCall);

class ReadCall: public RemoteCall
{
  public:
    ReadCall(const READ3args&);
    ~ReadCall();
    READ3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    READ3args args;
    READ3res res;
};
DEF_SMART_PTR(ReadCall);

class WriteCall: public RemoteCall
{
  public:
    WriteCall(const WRITE3args&);
    ~WriteCall();
    WRITE3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    WRITE3args args;
    WRITE3res res;
};
DEF_SMART_PTR(WriteCall);


class CreateCall: public RemoteCall
{
  public:
    CreateCall(const CREATE3args&);
    ~CreateCall();
    CREATE3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    CREATE3args args;
    CREATE3res res;
};
DEF_SMART_PTR(CreateCall);


class MkdirCall: public RemoteCall
{
  public:
    MkdirCall(const MKDIR3args&);
    ~MkdirCall();
    MKDIR3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    MKDIR3args args;
    MKDIR3res res;
};
DEF_SMART_PTR(MkdirCall);

class SymLinkCall: public RemoteCall
{
  public:
    SymLinkCall(const SYMLINK3args&);
    ~SymLinkCall();
    SYMLINK3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    SYMLINK3args args;
    SYMLINK3res res;
};
DEF_SMART_PTR(SymLinkCall);

class MknodCall: public RemoteCall
{
  public:
    MknodCall(const MKNOD3args&);
    ~MknodCall();
    MKNOD3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    MKNOD3args args;
    MKNOD3res res;
};
DEF_SMART_PTR(MknodCall);

class RemoveCall: public RemoteCall
{
  public:
    RemoveCall(const REMOVE3args&);
    ~RemoveCall();
    REMOVE3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    REMOVE3args args;
    REMOVE3res res;
};
DEF_SMART_PTR(RemoveCall);

class RmdirCall: public RemoteCall
{
  public:
    RmdirCall(const RMDIR3args&);
    ~RmdirCall();
    RMDIR3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    RMDIR3args args;
    RMDIR3res res;
};
DEF_SMART_PTR(RmdirCall);

class RenameCall: public RemoteCall
{
  public:
    RenameCall(const RENAME3args&);
    ~RenameCall();
    RENAME3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    RENAME3args args;
    RENAME3res res;
};
DEF_SMART_PTR(RenameCall);


class LinkCall: public RemoteCall
{
  public:
    LinkCall(const LINK3args&);
    ~LinkCall();
    LINK3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    LINK3args args;
    LINK3res res;
};
DEF_SMART_PTR(LinkCall);

class ReaddirCall: public RemoteCall
{
  public:
    ReaddirCall(const READDIR3args&);
    ~ReaddirCall();
    READDIR3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    READDIR3args args;
    READDIR3res res;
    Buffer* m_decodedStringBuffer;
};
DEF_SMART_PTR(ReaddirCall);


class ReaddirPlusCall : public RemoteCall
{
  public:
    ReaddirPlusCall(const READDIRPLUS3args&);
    ~ReaddirPlusCall();

    READDIRPLUS3res& getResult() { return res; }
  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    READDIRPLUS3args args;
    READDIRPLUS3res res;
    Buffer* m_decodedStringBuffer;
};
DEF_SMART_PTR(ReaddirPlusCall);

class FsstatCall: public RemoteCall
{
  public:
    FsstatCall(const FSSTAT3args&);
    ~FsstatCall();
    FSSTAT3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    FSSTAT3args args;
    FSSTAT3res res;
};
DEF_SMART_PTR(FsstatCall);

class FsinfoCall: public RemoteCall
{
  public:
    FsinfoCall(const FSINFO3args&);
    ~FsinfoCall();
    FSINFO3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    FSINFO3args args;
    FSINFO3res res;
};
DEF_SMART_PTR(FsinfoCall);

class PathConfCall: public RemoteCall
{
  public:
    PathConfCall(const PATHCONF3args&);
    ~PathConfCall();
    PATHCONF3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    PATHCONF3args args;
    PATHCONF3res res;
};
DEF_SMART_PTR(PathConfCall);

class CommitCall: public RemoteCall
{
  public:
    CommitCall(const COMMIT3args&);
    ~CommitCall();
    COMMIT3res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    COMMIT3args args;
    COMMIT3res res;
};
DEF_SMART_PTR(CommitCall);

}} // end of namespace
#endif /* _NFS_CALL_ */
