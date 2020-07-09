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

#ifndef _NLM_CALL_
#define _NLM_CALL_

#include "RpcCall.h"
#include <nfsrpc/nlm.h>

namespace OpenNfsC {

namespace NLMv4 {

class NullCall: public RemoteCall
{
  public:
    NullCall():RemoteCall(NLM, NLM_V4_NULL) {};
    ~NullCall() {};
  private:
    virtual int encodeArguments() { return 0; }
    virtual int decodeResults() { return 0; }
};

class TestCall : public RemoteCall
{
  public:
    TestCall(const nlm4_testargs&);
    ~TestCall();

    nlm4_testres& getResult() { return res; }
  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    nlm4_testargs args;
    nlm4_testres res;
};

class LockCall : public RemoteCall
{
  public:
    LockCall(const nlm4_lockargs&);
    ~LockCall() {};
    nlm4_res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    nlm4_lockargs args;
    nlm4_res res;
};

class CancelCall : public RemoteCall
{
  public:
    CancelCall(const nlm4_cancargs &);
    ~CancelCall() {};

    nlm4_res& getResult() { return res; }
  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;

  private:
    nlm4_cancargs args;
    nlm4_res res;
};

class UnlockCall : public RemoteCall
{
  public:
    UnlockCall(const nlm4_unlockargs&);
    ~UnlockCall() {};

    nlm4_res& getResult() { return res; }

  private:
    virtual int encodeArguments() ;
    virtual int decodeResults() ;
  private:
    nlm4_unlockargs args;
    nlm4_res res;
};

}} // end of namespace
#endif /* _NLM_CALL_ */
