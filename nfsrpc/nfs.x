/* Modified and unified nfs RPC header */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

const NFS_PORT          = 2049;
const NFS_MAXDATA       = 32768; /* not really correct */
const NFS_MAXPATHLEN    = 1024;
const NFS_MAXNAMLEN	= 255;
const NFS2_FHSIZE	= 32;
const NFS2_COOKIESIZE	= 4;
const NFS2_FIFO_DEV	= -1;	/* size kludge for named pipes */

/*
 * File types
 */
const NFSMODE_FMT  = 0170000;	/* type of file */
const NFSMODE_DIR  = 0040000;	/* directory */
const NFSMODE_CHR  = 0020000;	/* character special */
const NFSMODE_BLK  = 0060000;	/* block special */
const NFSMODE_REG  = 0100000;	/* regular */
const NFSMODE_LNK  = 0120000;	/* symbolic link */
const NFSMODE_SOCK = 0140000;	/* socket */
const NFSMODE_FIFO = 0010000;	/* fifo */

/*
 * Error status
 */
enum nfsstat2 {
	NFS2_OK= 0,		/* no error */
	NFS2ERR_PERM=1,		/* Not owner */
	NFS2ERR_NOENT=2,		/* No such file or directory */
	NFS2ERR_IO=5,		/* I/O error */
	NFS2ERR_NXIO=6,		/* No such device or address */
	NFS2ERR_ACCES=13,	/* Permission denied */
	NFS2ERR_EXIST=17,	/* File exists */
	NFSXERR_XDEV=18,	/* Cross-device link - solaris extension */
	NFS2ERR_NODEV=19,	/* No such device */
	NFS2ERR_NOTDIR=20,	/* Not a directory*/
	NFS2ERR_ISDIR=21,	/* Is a directory */
	NFSXERR_INVAL=22,	/* Invalid argument - solaris extension*/
	NFS2ERR_FBIG=27,		/* File too large */
	NFS2ERR_NOSPC=28,	/* No space left on device */
	NFS2ERR_ROFS=30,		/* Read-only file system */
	NFSXERR_OPNOTSUPP=45,	/* Operation not supported - solaris extension */
	NFS2ERR_NAMETOOLONG=63,	/* File name too long */
	NFS2ERR_NOTEMPTY=66,	/* Directory not empty */
	NFS2ERR_DQUOT=69,	/* Disc quota exceeded */
	NFS2ERR_STALE=70,	/* Stale NFS file handle */
	NFSXERR_REMOTE=71,	/* Object is remote - solaris extension */
	NFSXERR_WFLUSH=72,	/* write cache flushed - solaris extension */
	NFS2ERR_WFLUSH=99	/* write cache flushed - undefined */
};

/*
 * File types
 */
enum ftype2 {
	NF2NON = 0,	/* non-file */
	NF2REG = 1,	/* regular file */
	NF2DIR = 2,	/* directory */
	NF2BLK = 3,	/* block special */
	NF2CHR = 4,	/* character special */
	NF2LNK = 5,	/* symbolic link */
	NF2SOCK = 6,	/* unix domain sockets */
	NF2BAD = 7,	/* unused */
	NF2FIFO = 8 	/* named pipe */
};

/*
 * File access handle
 */
struct nfs_fh2 {
	opaque fh_data[NFS2_FHSIZE];
};

/* 
 * Timeval
 */
struct nfstime2 {
	unsigned time_seconds;
	unsigned time_useconds;
};


/*
 * File attributes
 */
struct fattr2 {
	ftype2 fattr2_type;		/* file type */
	unsigned fattr2_mode;		/* protection mode bits */
	unsigned fattr2_nlink;		/* # hard links */
	unsigned fattr2_uid;		/* owner user id */
	unsigned fattr2_gid;		/* owner group id */
	unsigned fattr2_size;		/* file size in bytes */
	unsigned fattr2_blocksize;	/* prefered block size */
	unsigned fattr2_rdev;		/* special device # */
	unsigned fattr2_blocks;	/* Kb of disk used by file */
	unsigned fattr2_fsid;		/* device # */
	unsigned fattr2_fileid;	/* inode # */
	nfstime2	fattr2_atime;		/* time of last access */
	nfstime2	fattr2_mtime;		/* time of last modification */
	nfstime2	fattr2_ctime;		/* time of last change */
};

/*
 * File attributes which can be set
 */
struct sattr2 {
	unsigned sattr2_mode;	/* protection mode bits */
	unsigned sattr2_uid;	/* owner user id */
	unsigned sattr2_gid;	/* owner group id */
	unsigned sattr2_size;	/* file size in bytes */
	nfstime2	sattr2_atime;	/* time of last access */
	nfstime2	sattr2_mtime;	/* time of last modification */
};


typedef string filename2<NFS_MAXNAMLEN>; 
typedef string nfspath2<NFS_MAXPATHLEN>;

/*
 * Reply status with file attributes
 */
union attrstat2 switch (nfsstat2 status) {
case NFS2_OK:
	fattr2 attrstat2;
default:
	void;
};

struct sattr2args {
	nfs_fh2 sattr2_file;
	sattr2 sattr2_attributes;
};

/*
 * Arguments for directory operations
 */
struct dirop2args {
	nfs_fh2	dirop2_dir;	/* directory file handle */
	filename2 dirop2_name;		/* name (up to NFS_MAXNAMLEN bytes) */
};

struct dirop2okres {
	nfs_fh2 dirop2_file;
	fattr2 dirop2_attributes;
};

/*
 * Results from directory operation
 */
union dirop2res switch (nfsstat2 status) {
case NFS2_OK:
	dirop2okres dirop2res;
default:
	void;
};

union readlink2res switch (nfsstat2 status) {
case NFS2_OK:
	nfspath2 readlink2_data;
default:
	void;
};

/*
 * Arguments to remote read
 */
struct read2args {
	nfs_fh2 read2_file;		/* handle for file */
	unsigned read2_offset;	/* byte offset in file */
	unsigned read2_count;		/* immediate read count */
	unsigned read2_totalcount;	/* total read count (from this offset)*/
};

/*
 * Status OK portion of remote read reply
 */
struct read2okres {
	fattr2	read2_attributes;	/* attributes, need for pagin*/
	opaque read2_data<NFS_MAXDATA>;
};

union read2res switch (nfsstat2 status) {
case NFS2_OK:
	read2okres read2_reply;
default:
	void;
};

/*
 * Arguments to remote write 
 */
struct write2args {
	nfs_fh2	write2_file;		/* handle for file */
	unsigned write2_beginoffset;	/* beginning byte offset in file */
	unsigned write2_offset;	/* current byte offset in file */
	unsigned write2_totalcount;	/* total write count (to this offset)*/
	opaque write2_data<NFS_MAXDATA>;
};

struct create2args {
	dirop2args create2_where;
	sattr2 create2_attributes;
};

struct rename2args {
	dirop2args rename2_from;
	dirop2args rename2_to;
};

struct link2args {
	nfs_fh2 link2_from;
	dirop2args link2_to;
};

struct symlink2args {
	dirop2args symlink2_from;
	nfspath2 symlink2_to;
	sattr2 symlink2_attributes;
};


typedef opaque nfscookie2[NFS2_COOKIESIZE];

/*
 * Arguments to readdir
 */
struct readdir2args {
	nfs_fh2 readdir2_dir;		/* directory handle */
	nfscookie2 readdir2_cookie;
	unsigned readdir2_count;		/* number of directory bytes to read */
};

struct entry2 {
	unsigned entry2_fileid;
	filename2 entry2_name;
	nfscookie2 entry2_cookie;
	entry2 *entry2_nextentry;
};

struct dirlist2 {
	entry2 *dirlist2_entries;
	bool dirlist2_eof;
};

union readdir2res switch (nfsstat2 status) {
case NFS2_OK:
	dirlist2 readdir2_reply;
default:
	void;
};

struct statfs2okres {
	unsigned statfs2_tsize;	/* preferred transfer size in bytes */
	unsigned statfs2_bsize;	/* fundamental file system block size */
	unsigned statfs2_blocks;	/* total blocks in file system */
	unsigned statfs2_bfree;	/* free blocks in fs */
	unsigned statfs2_bavail;	/* free blocks avail to non-superuser */
};

union statfs2res switch (nfsstat2 status) {
case NFS2_OK:
	statfs2okres statfs2_reply;
default:
	void;
};

/*
 * NFSv3 constants and types
 */

const NFS3_FHSIZE	= 64;	/* maximum size in bytes of a file handle */
const NFS3_COOKIEVERFSIZE = 8;	/* size of a cookie verifier for READDIR */
const NFS3_CREATEVERFSIZE = 8;	/* size of the verifier used for CREATE */
const NFS3_WRITEVERFSIZE = 8;	/* size of the verifier used for WRITE */

typedef string filename3<>;
typedef string nfspath3<>;
typedef unsigned hyper fileid3;
typedef unsigned hyper cookie3;
typedef opaque cookieverf3[NFS3_COOKIEVERFSIZE];
typedef opaque createverf3[NFS3_CREATEVERFSIZE];
typedef opaque writeverf3[NFS3_WRITEVERFSIZE];
typedef unsigned int uid3;
typedef unsigned int gid3;
typedef unsigned hyper size3;
typedef unsigned hyper offset3;
typedef unsigned int mode3;
typedef unsigned int count3;

/*
 * Error status (v3)
 */
enum nfsstat3 {
	NFS3_OK	= 0,
	NFS3ERR_PERM		= 1,
	NFS3ERR_NOENT		= 2,
	NFS3ERR_IO		= 5,
	NFS3ERR_NXIO		= 6,
	NFS3ERR_ACCES		= 13,
	NFS3ERR_EXIST		= 17,
	NFS3ERR_XDEV		= 18,
	NFS3ERR_NODEV		= 19,
	NFS3ERR_NOTDIR		= 20,
	NFS3ERR_ISDIR		= 21,
	NFS3ERR_INVAL		= 22,
	NFS3ERR_FBIG		= 27,
	NFS3ERR_NOSPC		= 28,
	NFS3ERR_ROFS		= 30,
	NFS3ERR_MLINK		= 31,
	NFS3ERR_NAMETOOLONG	= 63,
	NFS3ERR_NOTEMPTY	= 66,
	NFS3ERR_DQUOT		= 69,
	NFS3ERR_STALE		= 70,
	NFS3ERR_REMOTE		= 71,
	NFS3ERR_BADHANDLE	= 10001,
	NFS3ERR_NOT_SYNC	= 10002,
	NFS3ERR_BAD_COOKIE	= 10003,
	NFS3ERR_NOTSUPP		= 10004,
	NFS3ERR_TOOSMALL	= 10005,
	NFS3ERR_SERVERFAULT	= 10006,
	NFS3ERR_BADTYPE		= 10007,
	NFS3ERR_JUKEBOX		= 10008
};

/*
 * File types (v3)
 */
enum ftype3 {
	NF3NON	= 0,		/* invalid - local extension */
	NF3REG	= 1,		/* regular file */
	NF3DIR	= 2,		/* directory */
	NF3BLK	= 3,		/* block special */
	NF3CHR	= 4,		/* character special */
	NF3LNK	= 5,		/* symbolic link */
	NF3SOCK	= 6,		/* unix domain sockets */
	NF3FIFO	= 7		/* named pipe */
};

struct specdata3 {
	unsigned int	specdata1;
	unsigned int	specdata2;
};

/*
 * File access handle (v3)
 */
struct nfs_fh3 {
	opaque fh3_data<NFS3_FHSIZE>;
};

/* 
 * Timeval (v3)
 */
struct nfstime3 {
	unsigned int	time3_seconds;
	unsigned int	time3_nseconds;
};


/*
 * File attributes (v3)
 */
struct fattr3 {
	ftype3	fattr3_type;		/* file type */
	mode3	fattr3_mode;		/* protection mode bits */
	unsigned int	fattr3_nlink;		/* # hard links */
	uid3	fattr3_uid;		/* owner user id */
	gid3	fattr3_gid;		/* owner group id */
	size3	fattr3_size;		/* file size in bytes */
	size3	fattr3_used;		/* prefered block size */
	specdata3 fattr3_rdev;		/* special device # */
	unsigned hyper fattr3_fsid;		/* device # */
	fileid3	fattr3_fileid;		/* inode # */
	nfstime3 fattr3_atime;		/* time of last access */
	nfstime3 fattr3_mtime;		/* time of last modification */
	nfstime3 fattr3_ctime;		/* time of last change */
};

union post_op_attr switch (bool attributes_follow) {
case 1:
	fattr3	post_op_attr;
case 0:
	void;
};

struct wcc_attr {
	size3	wcc_size;
	nfstime3 wcc_mtime;
	nfstime3 wcc_ctime;
};

union pre_op_attr switch (bool attributes_follow) {
case 1:
	wcc_attr pre_op_attr;
case 0:
	void;
};

struct wcc_data {
	pre_op_attr wcc_before;
	post_op_attr wcc_after;
};

union post_op_fh3 switch (bool handle_follows) {
case 1:
	nfs_fh3	post_op_fh3;
case 0:
	void;
};

/*
 * File attributes which can be set (v3)
 */
enum time_how {
	TIME_DONT_CHANGE		= 0,
	TIME_SET_TO_SERVER_TIME	= 1,
	TIME_SET_TO_CLIENT_TIME	= 2
};

union set_mode3 switch (bool set_it) {
case 1:
	mode3	mode;
default:
	void;
};

union set_uid3 switch (bool set_it) {
case 1:
	uid3	uid;
default:
	void;
};

union set_gid3 switch (bool set_it) {
case 1:
	gid3	gid;
default:
	void;
};

union set_size3 switch (bool set_it) {
case 1:
	size3	size;
default:
	void;
};

union set_atime switch (time_how set_it) {
case TIME_SET_TO_CLIENT_TIME:
	nfstime3	atime;
default:
	void;
};

union set_mtime switch (time_how set_it) {
case TIME_SET_TO_CLIENT_TIME:
	nfstime3	mtime;
default:
	void;
};

struct sattr3 {
	set_mode3	sattr3_mode;
	set_uid3	sattr3_uid;
	set_gid3	sattr3_gid;
	set_size3	sattr3_size;
	set_atime	sattr3_atime;
	set_mtime	sattr3_mtime;
};

/*
 * Arguments for directory operations (v3)
 */
struct diropargs3 {
	nfs_fh3	dirop3_dir;		/* directory file handle */
	filename3 dirop3_name;		/* name (up to NFS_MAXNAMLEN bytes) */
};

/*
 * Arguments to getattr (v3).
 */
struct GETATTR3args {
	nfs_fh3		getattr3_object;
};

struct GETATTR3resok {
	fattr3		getattr3_obj_attributes;
};

union GETATTR3res switch (nfsstat3 status) {
case NFS3_OK:
	GETATTR3resok	getattr3ok;
default:
	void;
};

/*
 * Arguments to setattr (v3).
 */
union sattrguard3 switch (bool check) {
case 1:
	nfstime3	sattr3_obj_ctime;
case 0:
	void;
};

struct SETATTR3args {
	nfs_fh3		setattr3_object;
	sattr3		setattr3_new_attributes;
	sattrguard3	setattr3_guard;
};

struct SETATTR3resok {
	wcc_data	setattr3_obj_wcc;
};

struct SETATTR3resfail {
	wcc_data	setattr3fail_obj_wcc;
};

union SETATTR3res switch (nfsstat3 status) {
case NFS3_OK:
	SETATTR3resok	setattr3_ok;
default:
	SETATTR3resfail	setattr3fail;
};

/*
 * Arguments to lookup (v3).
 */
struct LOOKUP3args {
	diropargs3	lookup3_what;
};

struct LOOKUP3resok {
	nfs_fh3		lookup3_object;
	post_op_attr	lookup3_obj_attributes;
	post_op_attr	lookup3_dir_attributes;
};

struct LOOKUP3resfail {
	post_op_attr	lookup3fail_dir_attributes;
};

union LOOKUP3res switch (nfsstat3 status) {
case NFS3_OK:
	LOOKUP3resok	lookup3ok;
default:
	LOOKUP3resfail	lookup3fail;
};

/*
 * Arguments to access (v3).
 */
const ACCESS3_READ	= 0x0001;
const ACCESS3_LOOKUP	= 0x0002;
const ACCESS3_MODIFY	= 0x0004;
const ACCESS3_EXTEND	= 0x0008;
const ACCESS3_DELETE	= 0x0010;
const ACCESS3_EXECUTE	= 0x0020;

struct ACCESS3args {
	nfs_fh3		access3_object;
	unsigned int		access3_access;
};

struct ACCESS3resok {
	post_op_attr	access3_obj_attributes;
	unsigned int		access3_res;
};

struct ACCESS3resfail {
	post_op_attr	access3fail_obj_attributes;
};

union ACCESS3res switch (nfsstat3 status) {
case NFS3_OK:
	ACCESS3resok	access3ok;
default:
	ACCESS3resfail	access3fail;
};

/*
 * Arguments to readlink (v3).
 */
struct READLINK3args {
	nfs_fh3		readlink3_symlink;
};

struct READLINK3resok {
	post_op_attr	readlink3_symlink_attributes;
	nfspath3	readlink3_data;
};

struct READLINK3resfail {
	post_op_attr	readlink3fail_symlink_attributes;
};

union READLINK3res switch (nfsstat3 status) {
case NFS3_OK:
	READLINK3resok	readlink3ok;
default:
	READLINK3resfail readlink3fail;
};

/*
 * Arguments to read (v3).
 */
struct READ3args {
	nfs_fh3		read3_file;
	offset3		read3_offset;
	count3		read3_count;
};

struct READ3resok {
	post_op_attr	read3_file_attributes;
	count3		read3_count_res;
	bool		read3_eof;
	opaque		read3_data<>;
};

struct READ3resfail {
	post_op_attr	read3fail_file_attributes;
};

/* XXX: solaris 2.6 uses ``nfsstat'' here */
union READ3res switch (nfsstat3 status) {
case NFS3_OK:
	READ3resok	read3ok;
default:
	READ3resfail	read3fail;
};

/*
 * Arguments to write (v3).
 */
enum stable_how {
	STABLE_UNSTABLE	= 0,
	STABLE_DATA_SYNC	= 1,
	STABLE_FILE_SYNC	= 2
};

struct WRITE3args {
	nfs_fh3		write3_file;
	offset3		write3_offset;
	count3		write3_count;
	stable_how	write3_stable;
	opaque		write3_data<>;
};

struct WRITE3resok {
	wcc_data	write3_file_wcc;
	count3		write3_count_res;
	stable_how	write3_committed;
	writeverf3	write3_verf;
};

struct WRITE3resfail {
	wcc_data	write3fail_file_wcc;
};

union WRITE3res switch (nfsstat3 status) {
case NFS3_OK:
	WRITE3resok	write3ok;
default:
	WRITE3resfail	write3fail;
};

/*
 * Arguments to create (v3).
 */
enum createmode3 {
	CREATE_UNCHECKED	= 0,
	CREATE_GUARDED		= 1,
	CREATE_EXCLUSIVE	= 2
};

union createhow3 switch (createmode3 mode) {
case CREATE_UNCHECKED:
case CREATE_GUARDED:
	sattr3		create3_obj_attributes;
case CREATE_EXCLUSIVE:
	createverf3	create3_verf;
};

struct CREATE3args {
	diropargs3	create3_where;
	createhow3	create3_how;
};

struct CREATE3resok {
	post_op_fh3	create3_obj;
	post_op_attr	create3_obj_attributes;
	wcc_data	create3_dir_wcc;
};

struct CREATE3resfail {
	wcc_data	create3fail_dir_wcc;
};

union CREATE3res switch (nfsstat3 status) {
case NFS3_OK:
	CREATE3resok	create3_ok;
default:
	CREATE3resfail	create3fail;
};

/*
 * Arguments to mkdir (v3).
 */
struct MKDIR3args {
	diropargs3	mkdir3_where;
	sattr3		mkdir3_attributes;
};

struct MKDIR3resok {
	post_op_fh3	mkdir3_obj;
	post_op_attr	mkdir3_obj_attributes;
	wcc_data	mkdir3_dir_wcc;
};

struct MKDIR3resfail {
	wcc_data	mkdir3fail_dir_wcc;
};

union MKDIR3res switch (nfsstat3 status) {
case NFS3_OK:
	MKDIR3resok	mkdir3ok;
default:
	MKDIR3resfail	mkdir3fail;
};

/*
 * Arguments to symlink (v3).
 */
struct symlinkdata3 {
	sattr3		symlink3_attributes;
	nfspath3	symlink3_data;
};

struct SYMLINK3args {
	diropargs3	symlink3_where;
	symlinkdata3	symlink3_symlink;
};

struct SYMLINK3resok {
	post_op_fh3	symlink3_obj;
	post_op_attr	symlink3_obj_attributes;
	wcc_data	symlink3_dir_wcc;
};

struct SYMLINK3resfail {
	wcc_data	symlink3fail_dir_wcc;
};

union SYMLINK3res switch (nfsstat3 status) {
case NFS3_OK:
	SYMLINK3resok	symlink3ok;
default:
	SYMLINK3resfail	symlink3fail;
};

/*
 * Arguments to mknod (v3).
 */
struct devicedata3 {
	sattr3		dev3_attributes;
	specdata3	dev3_spec;
};

union mknoddata3 switch (ftype3 type) {
case NF3CHR:
case NF3BLK:
	devicedata3	mknod3_device;
case NF3SOCK:
case NF3FIFO:
	sattr3		mknod3_pipe_attributes;
default:
	void;
};

struct MKNOD3args {
	diropargs3	mknod3_where;
	mknoddata3	mknod3_what;
};

struct MKNOD3resok {
	post_op_fh3	mknod3_obj;
	post_op_attr	mknod3_obj_attributes;
	wcc_data	mknod3_dir_wcc;
};

struct MKNOD3resfail {
	wcc_data	mknod3fail_dir_wcc;
};

union MKNOD3res switch (nfsstat3 status) {
case NFS3_OK:
	MKNOD3resok	mknod3ok;
default:
	MKNOD3resfail	mknod3fail;
};

/*
 * Arguments to remove (v3).
 */
struct REMOVE3args {
	diropargs3	remove3_object;
};

struct REMOVE3resok {
	wcc_data	remove3_dir_wcc;
};

struct REMOVE3resfail {
	wcc_data	remove3fail_dir_wcc;
};

union REMOVE3res switch (nfsstat3 status) {
case NFS3_OK:
	REMOVE3resok	remove3ok;
default:
	REMOVE3resfail	remove3fail;
};

/*
 * Arguments to rmdir (v3).
 */
struct RMDIR3args {
	diropargs3	rmdir3_object;
};

struct RMDIR3resok {
	wcc_data	rmdir3_dir_wcc;
};

struct RMDIR3resfail {
	wcc_data	rmdir3fail_dir_wcc;
};

union RMDIR3res switch (nfsstat3 status) {
case NFS3_OK:
	RMDIR3resok	rmdir3ok;
default:
	RMDIR3resfail	rmdir3fail;
};

/*
 * Arguments to rename (v3).
 */
struct RENAME3args {
	diropargs3	rename3_from;
	diropargs3	rename3_to;
};

struct RENAME3resok {
	wcc_data	rename3_fromdir_wcc;
	wcc_data	rename3_todir_wcc;
};

struct RENAME3resfail {
	wcc_data	rename3fail_fromdir_wcc;
	wcc_data	rename3fail_todir_wcc;
};

union RENAME3res switch (nfsstat3 status) {
case NFS3_OK:
	RENAME3resok	rename3ok;
default:
	RENAME3resfail	rename3fail;
};

/*
 * Arguments to link (v3).
 */
struct LINK3args {
	nfs_fh3		link3_file;
	diropargs3	link3_link;
};

struct LINK3resok {
	post_op_attr	link3_file_attributes;
	wcc_data	link3_linkdir_wcc;
};

struct LINK3resfail {
	post_op_attr	link3fail_file_attributes;
	wcc_data	link3fail_linkdir_wcc;
};

union LINK3res switch (nfsstat3 status) {
case NFS3_OK:
	LINK3resok	link3ok;
default:
	LINK3resfail	link3fail;
};

/*
 * Arguments to readdir (v3).
 */
struct READDIR3args {
	nfs_fh3		readdir3_dir;
	cookie3		readdir3_cookie;
	cookieverf3	readdir3_cookieverf;
	count3		readdir3_count;
};

struct entry3 {
	fileid3		entry3_fileid;
	filename3	entry3_name;
	cookie3		entry3_cookie;
	entry3		*entry3_nextentry;
};

struct dirlist3 {
	entry3		*dirlist3_entries;
	bool		dirlist3_eof;
};

struct READDIR3resok {
	post_op_attr	readdir3_dir_attributes;
	cookieverf3	readdir3_cookieverf_res;
	dirlist3	readdir3_reply;
};

struct READDIR3resfail {
	post_op_attr	readdir3fail_dir_attributes;
};

union READDIR3res switch (nfsstat3 status) {
case NFS3_OK:
	READDIR3resok	readdir3ok;
default:
	READDIR3resfail	readdir3fail;
};

/*
 * Arguments to readdirplus (v3).
 */
struct READDIRPLUS3args {
	nfs_fh3		readdirplus3_dir;
	cookie3		readdirplus3_cookie;
	cookieverf3	readdirplus3_cookieverf;
	count3		readdirplus3_dircount;
	count3		readdirplus3_maxcount;
};

struct entryplus3 {
	fileid3		entryplus3_fileid;
	filename3	entryplus3_name;
	cookie3		entryplus3_cookie;
	post_op_attr	entryplus3_name_attributes;
	post_op_fh3	entryplus3_name_handle;
	entryplus3	*entryplus3_nextentry;
};

struct dirlistplus3 {
	entryplus3	*dirlistplus3_entries;
	bool		dirlistplus3_eof;
};

struct READDIRPLUS3resok {
	post_op_attr	readdirplus3_dir_attributes;
	cookieverf3	readdirplus3_cookieverf_res;
	dirlistplus3	readdirplus3_reply;
};

struct READDIRPLUS3resfail {
	post_op_attr	readdirplus3fail_dir_attributes;
};

union READDIRPLUS3res switch (nfsstat3 status) {
case NFS3_OK:
	READDIRPLUS3resok	readdirplus3ok;
default:
	READDIRPLUS3resfail	readdirplus3fail;
};

/*
 * Arguments to fsstat (v3).
 */
struct FSSTAT3args {
	nfs_fh3		fsstat3_fsroot;
};

struct fsstat3 {
	size3		fsstat3_tbytes;
	size3		fsstat3_fbytes;
	size3		fsstat3_abytes;
	size3		fsstat3_tfiles;
	size3		fsstat3_ffiles;
	size3		fsstat3_afiles;
};

struct FSSTAT3resok {
	post_op_attr	fsstat3_obj_attributes;
	fsstat3		fsstat3_fsstat3;
	unsigned int	fsstat3_invarsec;
};

struct FSSTAT3resfail {
	post_op_attr	fsstat3fail_obj_attributes;
};

union FSSTAT3res switch (nfsstat3 status) {
case NFS3_OK:
	FSSTAT3resok	fsstat3ok;
default:
	FSSTAT3resfail	fsstat3fail;
};

/*
 * Arguments to fsinfo (v3).
 */
const FSF3_LINK		= 0x0001;
const FSF3_SYMLINK	= 0x0002;
const FSF3_HOMOGENEOUS	= 0x0008;
const FSF3_CANSETTIME	= 0x0010;

struct FSINFO3args {
	nfs_fh3		fsinfo3_fsroot;
};

struct fsinfo3 {
	unsigned int		fsinfo3_rtmax;
	unsigned int		fsinfo3_rtpref;
	unsigned int		fsinfo3_rtmult;
	unsigned int		fsinfo3_wtmax;
	unsigned int		fsinfo3_wtpref;
	unsigned int		fsinfo3_wtmult;
	unsigned int		fsinfo3_dtpref;
	size3		fsinfo3_maxfilesize;
	nfstime3	fsinfo3_time_delta;
	unsigned int		fsinfo3_properties;
};

struct FSINFO3resok {
	post_op_attr	fsinfo3_obj_attributes;
	fsinfo3		fsinfo3_fsinfo3;
};

struct FSINFO3resfail {
	post_op_attr	fsinfo3fail_obj_attributes;
};

union FSINFO3res switch (nfsstat3 status) {
case NFS3_OK:
	FSINFO3resok	fsinfo3ok;
default:
	FSINFO3resfail	fsinfo3fail;
};

/*
 * Arguments to pathconf (v3).
 */
struct PATHCONF3args {
	nfs_fh3		pathconf3_object;
};

struct PATHCONF3resok {
	post_op_attr	pathconf3_obj_attributes;
	unsigned int		pathconf3_linkmax;
	unsigned int		pathconf3_name_max;
	bool		pathconf3_no_trunc;
	bool		pathconf3_chown_restricted;
	bool		pathconf3_case_insensitive;
	bool		pathconf3_case_preserving;
};

struct PATHCONF3resfail {
	post_op_attr	pathconf3fail_obj_attributes;
};

union PATHCONF3res switch (nfsstat3 status) {
case NFS3_OK:
	PATHCONF3resok	pathconf3ok;
default:
	PATHCONF3resfail	pathconf3fail;
};

/*
 * Arguments to commit (v3).
 */
struct COMMIT3args {
	nfs_fh3		commit3_file;
	offset3		commit3_offset;
	count3		commit3_count;
};

struct COMMIT3resok {
	wcc_data	commit3_file_wcc;
	writeverf3	commit3_verf;
};

struct COMMIT3resfail {
	wcc_data	commit3fail_file_wcc;
};

union COMMIT3res switch (nfsstat3 status) {
case NFS3_OK:
	COMMIT3resok	commit3ok;
default:
	COMMIT3resfail	commit3fail;
};

/*
 * Remote file service routines
 */
program RPCPROG_NFS {
	version NFS_V2 {
		void 
		NFS_V2_NULL(void) = 0;

		attrstat2 
		NFS_V2_GETATTR(nfs_fh2) =	1;

		attrstat2
		NFS_V2_SETATTR(sattr2args) = 2;

		void 
		NFS_V2_ROOT(void) = 3;

		dirop2res 
		NFS_V2_LOOKUP(dirop2args) = 4;

		readlink2res 
		NFS_V2_READLINK(nfs_fh2) = 5;

		read2res 
		NFS_V2_READ(read2args) = 6;

		void 
		NFS_V2_WRITECACHE(void) = 7;

		attrstat2
		NFS_V2_WRITE(write2args) = 8;

		dirop2res
		NFS_V2_CREATE(create2args) = 9;

		nfsstat2
		NFS_V2_REMOVE(dirop2args) = 10;

		nfsstat2
		NFS_V2_RENAME(rename2args) = 11;

		nfsstat2
		NFS_V2_LINK(link2args) = 12;

		nfsstat2
		NFS_V2_SYMLINK(symlink2args) = 13;

		dirop2res
		NFS_V2_MKDIR(create2args) = 14;

		nfsstat2
		NFS_V2_RMDIR(dirop2args) = 15;

		readdir2res
		NFS_V2_READDIR(readdir2args) = 16;

		statfs2res
		NFS_V2_STATFS(nfs_fh2) = 17;
	} = 2;

	version NFS_V3 {
		void
		NFS_V3_NULL(void)			= 0;

		GETATTR3res
		NFS_V3_GETATTR(GETATTR3args)		= 1;

		SETATTR3res
		NFS_V3_SETATTR(SETATTR3args)		= 2;

		LOOKUP3res
		NFS_V3_LOOKUP(LOOKUP3args)		= 3;

		ACCESS3res
		NFS_V3_ACCESS(ACCESS3args)		= 4;

		READLINK3res
		NFS_V3_READLINK(READLINK3args)	= 5;

		READ3res
		NFS_V3_READ(READ3args)		= 6;

		WRITE3res
		NFS_V3_WRITE(WRITE3args)		= 7;

		CREATE3res
		NFS_V3_CREATE(CREATE3args)		= 8;

		MKDIR3res
		NFS_V3_MKDIR(MKDIR3args)		= 9;

		SYMLINK3res
		NFS_V3_SYMLINK(SYMLINK3args)		= 10;

		MKNOD3res
		NFS_V3_MKNOD(MKNOD3args)		= 11;

		REMOVE3res
		NFS_V3_REMOVE(REMOVE3args)		= 12;

		RMDIR3res
		NFS_V3_RMDIR(RMDIR3args)		= 13;

		RENAME3res
		NFS_V3_RENAME(RENAME3args)		= 14;

		LINK3res
		NFS_V3_LINK(LINK3args)		= 15;

		READDIR3res
		NFS_V3_READDIR(READDIR3args)		= 16;

		READDIRPLUS3res
		NFS_V3_READDIRPLUS(READDIRPLUS3args)	= 17;

		FSSTAT3res
		NFS_V3_FSSTAT(FSSTAT3args)		= 18;

		FSINFO3res
		NFS_V3_FSINFO(FSINFO3args)		= 19;

		PATHCONF3res
		NFS_V3_PATHCONF(PATHCONF3args)	= 20;

		COMMIT3res
		NFS_V3_COMMIT(COMMIT3args)		= 21;
	} = 3;
} = 100003;

