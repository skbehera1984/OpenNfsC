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

#ifndef _NFS_APIHANDLE_H_
#define _NFS_APIHANDLE_H_

#include <SmartPtr.h>
#include "NfsConnectionGroup.h"

namespace OpenNfsC {

class NfsConnectionGroup;

class NfsApiHandle;
typedef SmartPtr<NfsApiHandle> NfsApiHandlePtr;

class NfsApiHandle : public SmartRef
{
  public:
    NfsApiHandle(NfsConnectionGroup *ptr) { m_pConn = ptr; }
    virtual ~NfsApiHandle() {}

  public:
    virtual bool connect(std::string &serverIP) = 0;
    virtual bool getRootFH(const std::string &nfs_export) = 0;
    virtual bool getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH) = 0;
    virtual bool getDirFh(const std::string &dirPath, NfsFh &dirFH) = 0;
    virtual bool open(const std::string   filePath,
                      uint32_t            access,
                      uint32_t            shareAccess,
                      uint32_t            shareDeny,
                      NfsFh              &file) = 0;
    virtual bool read(NfsFh              &fileFH,
                      uint64_t            offset,
                      uint32_t            length,
                      std::string        &data,
                      uint32_t           &bytesRead,
                      bool               &eof) = 0;
    virtual bool write(NfsFh              &fileFH,
                       uint64_t            offset,
                       std::string        &data,
                       uint32_t           &bytesWritten,
                       NfsAttr            &postAttr) = 0;
    virtual bool write_unstable(NfsFh              &fileFH,
                                uint64_t            offset,
                                std::string        &data,
                                uint32_t           &bytesWritten,
                                NfsAttr            &postAttr) = 0;
    virtual bool close(NfsFh &fileFh, NfsAttr &postAttr) = 0;
    virtual bool remove(std::string path) = 0;
    virtual bool remove(const NfsFh &parentFH, const string &name) = 0;
    virtual bool rename(const std::string  &nfs_export,
                        const std::string  &fromPath,
                        const std::string  &toPath) = 0;
    virtual bool readDir(const std::string &dirPath, NfsFiles &files) = 0;
    virtual bool truncate(NfsFh &fh, uint64_t size) = 0;
    virtual bool truncate(const std::string &path, uint64_t size) = 0;
    virtual bool access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc) = 0;
    virtual bool mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode) = 0;
    virtual bool mkdir(const std::string &path, uint32_t mode, bool createPath = false) = 0;
    virtual bool rmdir(const std::string &path) = 0;
    virtual bool commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf) = 0;
    virtual bool lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, bool reclaim = false) = 0;
    virtual bool unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length) = 0;
    virtual bool setattr( NfsFh &fh, NfsAttr &attr) =0;

  protected:
    NfsConnectionGroup *m_pConn;
};

}

#endif /* _NFS_APIHANDLE_H_ */
