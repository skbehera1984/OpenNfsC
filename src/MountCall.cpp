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

#include "MountCall.h"
#include "RpcPacket.h"
#include "RpcConnection.h"
#include "NfsUtil.h"
#include <iostream>
#include <vector>
#include <utility>
#include <syslog.h>

namespace OpenNfsC {
namespace Mount {

MntCall::MntCall(const dirpath& path):RemoteCall(MOUNT, MOUNT_V3_MNT),args(path),res()
{
}

MntCall::~MntCall()
{
  syslog(LOG_DEBUG, "enter MntCall::~MntCall\n");
}

int MntCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
    return request->xdrEncodeString((unsigned char*)args, strlen((const char*)args));
  else
    return -1;
}

int MntCall::decodeResults()
{
  uint32 status;
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  RETURN_ON_ERROR(reply->xdrDecodeUint32(&status));
  res.fhs_status = (enum mountstat3)status;
  if (status == 0)
  {
    RETURN_ON_ERROR(NfsUtil::decodeFH3(reply, &res.mountres3_u.mount3_mountinfo.mount3_fhandle));
  }
  return 0;
}

UMountCall::UMountCall(const dirpath& path):RemoteCall(MOUNT, MOUNT_V3_UMNT),args(path)
{
}

int UMountCall::encodeArguments()
{
  RpcPacketPtr request = getRequest();
  if (request)
    return request->xdrEncodeString((unsigned char*)args, strlen((const char*)args));
  return -1;
}

int UMountCall::decodeResults()
{
  return 0;
}

DumpCall::~DumpCall()
{
  if (res)
  {
    delete [] res;
    res = NULL;
  }

  if (m_decodedStringBuffer)
  {
    delete m_decodedStringBuffer;
    m_decodedStringBuffer = NULL;
  }
}

int DumpCall::encodeArguments()
{
  return 0;
}

int DumpCall::decodeResults()
{
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  uint32 entryFollow;
  RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));

  /* allocate a buffer large enough to hold all null-teminrated strings */
  m_decodedStringBuffer = new Buffer(reply->getCapacity());
  if (m_decodedStringBuffer == NULL)
    return -1;

  std::vector< std::pair<char*, char*> > dumpVec;
  while (entryFollow != 0)
  {
    char* hostname = reinterpret_cast<char*>(m_decodedStringBuffer->end());
    RETURN_ON_ERROR(NfsUtil::decodeStringToBuffer(reply, m_decodedStringBuffer));

    char* pathname = reinterpret_cast<char*>(m_decodedStringBuffer->end());
    RETURN_ON_ERROR(NfsUtil::decodeStringToBuffer(reply, m_decodedStringBuffer));

    dumpVec.push_back(std::make_pair(hostname, pathname));
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));
  }

  // populate the result data structure
  if (!dumpVec.empty())
  {
    int listsize = dumpVec.size();
    res = new mountbody[listsize];
    for (int i = 0; i < listsize; i++)
    {
      res[i].ml_hostname = dumpVec[i].first;
      res[i].ml_directory = dumpVec[i].second;
      if ( i == (listsize - 1) )
        res[i].ml_next = NULL;
      else
        res[i].ml_next = &res[i+1];
    }
  }
  return 0;
}

ExportCall::ExportCall():RemoteCall(MOUNT, MOUNT_V3_EXPORT),res(NULL), m_groupBuffer(NULL), m_decodedStringBuffer(NULL)
{
}

ExportCall::~ExportCall()
{
  if (res)
  {
    delete [] res;
  }

  if (m_groupBuffer)
    delete [] m_groupBuffer;

  if (m_decodedStringBuffer)
    delete m_decodedStringBuffer;
}

int ExportCall::encodeArguments()
{
  return 0;
}

int ExportCall::decodeResults()
{
  RpcPacketPtr reply = getReply();

  if (reply == NULL)
    return -1;

  uint32 entryFollow;
  RETURN_ON_ERROR(reply->xdrDecodeUint32(&entryFollow));

  m_decodedStringBuffer = new Buffer(reply->getCapacity());

  std::vector<char*> stringVector;

  int numExports = 0;
  int numGroups = 0;

  while (entryFollow)
  {
    char* dirName = NULL;
    dirName = reinterpret_cast<char*>(m_decodedStringBuffer->end());
    RETURN_ON_ERROR(NfsUtil::decodeStringToBuffer(reply, m_decodedStringBuffer));
    stringVector.push_back(dirName);
    ++numExports;

    uint32 groupfollow = 0;
    syslog(LOG_DEBUG, "ExportCall get export dir=%s\n", dirName);
    RETURN_ON_ERROR(reply->xdrDecodeUint32(&groupfollow));
    while (groupfollow)
    {
      char* name = NULL;
      name = reinterpret_cast<char*>(m_decodedStringBuffer->end());
      RETURN_ON_ERROR(NfsUtil::decodeStringToBuffer(reply, m_decodedStringBuffer));
      stringVector.push_back(name);

      ++numGroups;

      RETURN_ON_ERROR(reply->xdrDecodeUint32(&groupfollow));
    }

    stringVector.push_back(NULL);  // delimiter
    reply->xdrDecodeUint32(&entryFollow);
  }

  if (stringVector.size() > 0)
  {
    res = new exportnode[numExports];
    m_groupBuffer = new groupnode[numGroups];
    std::vector<char*>::iterator iter = stringVector.begin();
    int exportIndex = 0;
    int groupIndex = 0;
    groupnode* lastgroup = NULL;
    bool isGroup = false;
    while (iter != stringVector.end() && (exportIndex < numExports) )
    {
      if (!isGroup)
      {
        res[exportIndex].ex_dir = *iter;
        res[exportIndex].ex_groups = NULL;
        res[exportIndex].ex_next = (exportIndex == numExports -1) ? NULL : &res[exportIndex+1];
        isGroup = true;
        lastgroup = NULL;
      }
      else //in group
      {
        if (*iter == NULL) //zero group or last group
        {
          isGroup = false;
          ++exportIndex;
        }
        else
        {
          if (groupIndex >= numGroups)
            continue;

          groupnode* currentGroup = m_groupBuffer + groupIndex;
          currentGroup->gr_name = *iter;
          currentGroup->gr_next = NULL;

          if (lastgroup != NULL)
            lastgroup->gr_next = currentGroup;
          else
            res[exportIndex].ex_groups = currentGroup;

          lastgroup = currentGroup;
          ++groupIndex;
        }
      }
      ++iter;
    }
  }

  return 0;
}

}}
