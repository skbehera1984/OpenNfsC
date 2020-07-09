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

%#include <nfsrpc/nfs.h>

struct auth_sys_params {
	unsigned auth_sys_stamp;
	string auth_sys_machinename<255>;
	unsigned auth_sys_uid;
	unsigned auth_sys_gid;
	unsigned auth_sys_gids<16>;
};

/*
 * If a status of zero is returned, the call completed successfully, and 
 * a file handle for the directory follows. A non-zero status indicates
 * some sort of error. The status corresponds with UNIX error numbers.
 */
union fh2status switch (unsigned fhs_status) {
case 0:
	nfs_fh2 fhs_fh;
default:
	void;
};

/*
 * The type dirpath is the pathname of a directory
 */
typedef string dirpath<NFS_MAXPATHLEN>;

/*
 * The type name is used for arbitrary names (hostnames, groupnames)
 */
typedef string name<NFS_MAXNAMLEN>;

/*
 * A list of who has what mounted
 */
typedef struct mountbody *mountlist;
struct mountbody {
	name ml_hostname;
	dirpath ml_directory;
	mountlist ml_next;
};

/*
 * A list of netgroups
 */
typedef struct groupnode *groups;
struct groupnode {
	name gr_name;
	groups gr_next;
};

/*
 * A list of what is exported and to whom
 */
typedef struct exportnode *exports;
struct exportnode {
	dirpath ex_dir;
	groups ex_groups;
	exports ex_next;
};

/*
 * Status codes returned by the version 3 mount call.
 */
enum mountstat3 {
	MNT3_OK = 0,                 /* no error */
	MNT3ERR_PERM = 1,            /* Not owner */
	MNT3ERR_NOENT = 2,           /* No such file or directory */
	MNT3ERR_IO = 5,              /* I/O error */
	MNT3ERR_ACCES = 13,          /* Permission denied */
	MNT3ERR_NOTDIR = 20,         /* Not a directory */
	MNT3ERR_INVAL = 22,          /* Invalid argument */
	MNT3ERR_NAMETOOLONG = 63,    /* Filename too long */
	MNT3ERR_NOTSUPP = 10004,     /* Operation not supported */
	MNT3ERR_SERVERFAULT = 10006  /* A failure on the server */
};

struct mountres3_ok {
	nfs_fh3	mount3_fhandle;
	int	mount3_auth_flavors<>;
};

union mountres3 switch (mountstat3 fhs_status) {
case 0:
	mountres3_ok	mount3_mountinfo;
default:
	void;
};

program RPCPROG_MOUNT {
	/*
	 * Version one of the mount protocol communicates with version two
	 * of the NFS protocol. Version three communicates with
	 * version three of the NFS protocol. The only connecting
	 * point is the fhandle structure, which is the same for both
	 * protocols.
	 */
	version MOUNT_V2 {
		/*
		 * Does no work. It is made available in all RPC services
		 * to allow server reponse testing and timing
		 */
		void
		MOUNT_V2_NULL(void) = 0;

		/*	
		 * If fhs_status is 0, then fhs_fhandle contains the
	 	 * file handle for the directory. This file handle may
		 * be used in the NFS protocol. This procedure also adds
		 * a new entry to the mount list for this client mounting
		 * the directory.
		 * Unix authentication required.
		 */
		fh2status 
		MOUNT_V2_MNT(dirpath) = 1;

		/*
		 * Returns the list of remotely mounted filesystems. The 
		 * mountlist contains one entry for each hostname and 
		 * directory pair.
		 */
		mountlist
		MOUNT_V2_DUMP(void) = 2;

		/*
		 * Removes the mount list entry for the directory
		 * Unix authentication required.
		 */
		void
		MOUNT_V2_UMNT(dirpath) = 3;

		/*
		 * Removes all of the mount list entries for this client
		 * Unix authentication required.
		 */
		void
		MOUNT_V2_UMNTALL(void) = 4;

		/*
		 * Returns a list of all the exported filesystems, and which
		 * machines are allowed to import it.
		 */
		exports
		MOUNT_V2_EXPORT(void)  = 5;

		/*
		 * Identical to MOUNTPROC_EXPORT above
		 */
		exports
		MOUNT_V2_EXPORTALL(void) = 6;
	} = 1;

	version MOUNT_V3 {
		/*
		 * Does no work. It is made available in all RPC services
		 * to allow server reponse testing and timing
		 */
		void
		MOUNT_V3_NULL(void) = 0;

		/*
		 * If mountres3.fhs_status is MNT3_OK, then
		 * mountres3.mountinfo contains the file handle for
		 * the directory and a list of acceptable
		 * authentication flavors.  This file handle may only
		 * be used in the NFS version 3 protocol.  This
		 * procedure also results in the server adding a new
		 * entry to its mount list recording that this client
		 * has mounted the directory. AUTH_UNIX authentication
		 * or better is required.
		 */
		mountres3
		MOUNT_V3_MNT(dirpath) = 1;

		/*
		 * Returns the list of remotely mounted filesystems. The 
		 * mountlist contains one entry for each hostname and 
		 * directory pair.
		 */
		mountlist
		MOUNT_V3_DUMP(void) = 2;

		/*
		 * Removes the mount list entry for the directory
		 * Unix authentication required.
		 */
		void
		MOUNT_V3_UMNT(dirpath) = 3;

		/*
		 * Removes all of the mount list entries for this client
		 * Unix authentication required.
		 */
		void
		MOUNT_V3_UMNTALL(void) = 4;

		/*
		 * Returns a list of all the exported filesystems, and which
		 * machines are allowed to import it.
		 */
		exports
		MOUNT_V3_EXPORT(void)  = 5;
	} = 3;
} = 100005;
