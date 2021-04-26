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
#include <list>
#include <vector>

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
    virtual bool getExports(list<string>& Exports) = 0;
    virtual bool getRootFH(const std::string &nfs_export, NfsFh &rootFh, NfsError &status) = 0;
    virtual bool getDirFh(const NfsFh &rootFH, const std::string &dirPath, NfsFh &dirFH, NfsError &status) = 0;
    virtual bool getDirFh(const std::string &dirPath, NfsFh &dirFH, NfsError &status) = 0;
    virtual bool getFileHandle(NfsFh &rootFH, const std::string path, NfsFh &fileFh, NfsAttr &attr, NfsError &status) = 0;
    virtual bool create(NfsFh             &dirFh,
                        std::string       &fileName,
                        NfsAttr           *inAttr,
                        NfsFh             &fileFh,
                        NfsAttr           &outAttr,
                        NfsError          &status) = 0;
    virtual bool open(NfsFh             &rootFh,
                      const std::string  filePath,
                      NfsFh             &fileFh,
                      NfsAttr           &fileAttr,
                      NfsError          &status) = 0;
    virtual bool open(const std::string   filePath,
                      uint32_t            access,
                      uint32_t            shareAccess,
                      uint32_t            shareDeny,
                      NfsFh              &file,
                      NfsError           &status) = 0;
    virtual bool read(NfsFh              &fileFH,
                      uint64_t            offset,
                      uint32_t            length,
                      std::string        &data,
                      uint32_t           &bytesRead,
                      bool               &eof,
                      NfsAttr            &postAttr,
                      NfsError           &status) = 0;
    virtual bool write(NfsFh              &fileFH,
                       uint64_t            offset,
                       uint32_t            length,
                       std::string        &data,
                       uint32_t           &bytesWritten,
                       NfsError           &status) = 0;
    virtual bool write_unstable(NfsFh              &fileFH,
                                uint64_t            offset,
                                std::string        &data,
                                uint32_t           &bytesWritten,
                                char               *verf,
                                const bool          needverify,
                                NfsError           &status) = 0;
    virtual bool close(NfsFh &fileFh, NfsAttr &postAttr, NfsError &status) = 0;
    virtual bool remove(std::string &exp, std::string path, NfsError &status) = 0;
    virtual bool remove(const NfsFh &parentFH, const string &name, NfsError &status) = 0;
    virtual bool rename(NfsFh &fromDirFh,
                        const std::string &fromName,
                        NfsFh &toDirFh,
                        const std::string toName,
                        NfsError &status) = 0;
    virtual bool rename(const std::string  &nfs_export,
                        const std::string  &fromPath,
                        const std::string  &toPath,
                        NfsError           &status) = 0;
    virtual bool readDir(std::string &exp, const std::string &dirPath, NfsFiles &files, NfsError &status) = 0;
    virtual bool readDir(NfsFh &dirFh, NfsFiles &files, NfsError &status) = 0;
    virtual bool truncate(NfsFh &fh, uint64_t size, NfsError &status) = 0;
    virtual bool truncate(const std::string &path, uint64_t size, NfsError &status) = 0;
    virtual bool access(const std::string &filePath, uint32_t accessRequested, NfsAccess &acc, NfsError &status) = 0;
    virtual bool mkdir(const NfsFh &parentFH, const std::string dirName, uint32_t mode, NfsFh &dirFH, NfsError &status) = 0;
    virtual bool mkdir(const std::string &path, uint32_t mode, NfsError &status, bool createPath = false) = 0;
    virtual bool rmdir(std::string &exp, const std::string &path, NfsError &status) = 0;
    virtual bool rmdir(const NfsFh &parentFH, const string &name, NfsError &status) = 0;
    virtual bool commit(NfsFh &fh, uint64_t offset, uint32_t bytes, char *writeverf, NfsError &status) = 0;
    virtual bool lock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status, bool reclaim = false) = 0;
    virtual bool unlock(NfsFh &fh, uint32_t lockType, uint64_t offset, uint64_t length, NfsError &status) = 0;
    virtual bool setattr(NfsFh &fh, NfsAttr &attr, NfsError &status) =0;
    virtual bool getAttr(NfsFh &fh, NfsAttr &attr, NfsError &status) = 0;
    virtual bool getAttr(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status) = 0;
    virtual bool fileExists(const std::string& exp, const std::string& path, NfsAttr& attr, NfsError& status) = 0;
    virtual bool lookupPath(const std::string &exp_path, const std::string &pathFromRoot, NfsFh &lookup_fh, NfsAttr &lookup_attr, NfsError &status) = 0;
    virtual bool lookupPath(NfsFh &rootFh, const std::string &pathFromRoot, NfsFh &lookup_fh, NfsAttr &lookup_attr, NfsError &status) = 0;
    virtual bool lookup(const std::string &path, NfsFh &lookup_fh, NfsError &status) = 0;
    virtual bool lookup(NfsFh &dirFh, const std::string &file, NfsFh &lookup_fh, NfsAttr &attr, NfsError &status) = 0;
    virtual bool fsstat(NfsFh &rootFh, NfsFsStat &stat, uint32 &invarSec, NfsError &status) = 0;
    virtual bool link(NfsFh &tgtFh, NfsFh &parentFh, const string &linkName, NfsError &status) = 0;
    virtual bool symlink(const string &tgtPath, NfsFh &parentFh, const string &linkName, NfsError &status) = 0;

    virtual bool renewCid() = 0;

  protected:
    NfsConnectionGroup *m_pConn;
};

}

#endif /* _NFS_APIHANDLE_H_ */
