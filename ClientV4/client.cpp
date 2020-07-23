#include "ConnectionMgr.h"
#include "NfsConnectionGroup.h"
#include "RpcConnection.h"
#include "MountCall.h"
#include "NfsCall.h"
#include "Nfs4Call.h"
#include "NlmCall.h"
#include "RpcDefs.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

using namespace std;
using namespace OpenNfsC;

int main(int argc, char* argv[])
{
  std::string server, resource;
  int count = 2;

  if ( argc >= 2 )
    server = argv[1];
  if ( argc >=3 )
    resource = argv[2];
  if ( argc >=4 )
  {
    // can use std::stoi
    try { count = atoi(argv[3]); } catch (...) {}
    if ( count < 1 ) count = 1;
  }

  if ( server.empty() || resource.empty() )
  {
    cerr << "Usage: " << argv[0] << " <serverOrIP> <resource> [COUNT]" << endl;
    return -1;
  }

  struct in_addr ip = {0};
  inet_aton(server.c_str(), &ip);

  NfsError status;
  NfsConnectionGroupPtr svr4Ptr = NfsConnectionGroup::create(server/*ip.s_addr*/, TRANSP_TCP, NFSV4);
  if (svr4Ptr->connect(server, status) != false)
  {
    cout << "NFSV4 connect successful" << endl;
  }

  std::string path = resource;
  NfsFiles files;
  if (svr4Ptr->readDir(path, files, status) != false)
  {
    cout << "NFSV4 READDIR successful" << endl;
  }

  for (const NfsFile &file : files)
  {
    std::string type;
    if (file.type == FILE_TYPE_REG)
      type = "file";
    else if (file.type == FILE_TYPE_DIR)
      type = "directory";
    cout << file.name << ":"<< type <<endl;
  }

  cout << "No of files - " << files.size() <<endl;

#if 0
  NfsFh fileFH;
  std::string file = "/fs_nfsv4/sarat/dir1/xyz.txt";
  if (svr4Ptr->open(file, ACCESS4_READ|ACCESS4_MODIFY, SHARE_ACCESS_READ|SHARE_ACCESS_WRITE, SHARE_DENY_NONE, fileFH) != false)
  {
    cout << "NFSV4 OPEN successful" << endl;
  }

  NfsAttr attr;
  std::string write = "i am writing more data";
  uint32_t bytesWritten = 0;
  if (svr4Ptr->write(fileFH, 0, write, bytesWritten, attr) != false)
  {
    cout << "NFSV4 WRItE successful, bytes written - " << bytesWritten << endl;
  }

  if (svr4Ptr->write_unstable(fileFH, 0, write, bytesWritten, attr) != false)
  {
    cout << "NFSV4 WRItE successful, bytes written - " << bytesWritten << endl;
  }

  // For testing commit we need to make write in UNSTABLE4 option.
  char  var[8];
  if (svr4Ptr->commit(fileFH ,0, 10, var ) != false)
  {
    cout << " NFSV4 COMMIT successful" << var <<endl;
  }

  // For testing setattr
  NfsAttr attr;
  attr.mask[0]= (1 << FATTR4_SIZE);
  attr.mask[1]=0;
  attr.size =421;
  if (svr4Ptr->setattr(fileFH, attr) != false)
  {
    cout << "NFSV4 setattr successful " << endl;
  }


  std::string data;
  bool eof = false;
  uint32_t bytesRead = 0;
  if (svr4Ptr->read(fileFH, 0, 95, data, bytesRead, eof) != false)
  {
    cout << "NFSV4 READ successful. Bytes Read - " << bytesRead << endl;
    cout << "FILE DATA IS "<< endl << endl << data <<endl;
  }

  if (svr4Ptr->close(fileFH, attr) != false)
  {
    cout << "NFSV4 CLOSE successful" << endl;
  }

  attr.print();

  std::string file = "/NFSv4/rabi/aa.txt";
  NfsAccess acc;
  if (svr4Ptr->access(file, ACCESS4_READ|ACCESS4_MODIFY, acc ) != false)
  {
    cout << " NFSV4 ACCESS successful" << endl;
    cout << " Access supported = "<< acc.supported <<endl;
    cout << " Access granted = "<< acc.access <<endl;
  }

  if (svr4Ptr->rename("/fs_nfsv4", "skb.tar", "xyz/abc.txt") != false)
  {
    cout << "NFSV4 RENAME successful" << endl;
  }
  if (svr4Ptr->remove("/fs_nfsv4/sarat/dir1/dir2") != false)
  {
    cout << "NFSV4 REMOVE successful" << endl;
  }

  std::string file = "/fs_nfsv4/sarat/xyz.txt";
  if (svr4Ptr->truncate(file, 23) != false)
  {
    cout << "NFSV4 TRUNC successful" << endl;
  }

  NfsFh fileFH;
  std::string file = "/fs_nfsv4/sarat/dir1/xyz.txt";
  if (svr4Ptr->open(file, ACCESS4_READ|ACCESS4_MODIFY, SHARE_ACCESS_BOTH, SHARE_DENY_NONE, fileFH) != false)
  {
    cout << "NFSV4 OPEN successful" << endl;
  }
  if (svr4Ptr->lock(fileFH, WRITE_LOCK, 0, 0xffffffffffffffff) != false)
  {
    cout << "NFSV4 LOCK successful" << endl;
  }
  if (svr4Ptr->unlock(fileFH, WRITE_LOCK, 0, 0xffffffffffffffff) != false)
  {
    cout << "NFSV4 UNLOCK successful" << endl;
  }

#endif

  return 0;
}
