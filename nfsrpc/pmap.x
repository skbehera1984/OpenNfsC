/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
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

const PMAPPORT = 111;

struct pmap {
	unsigned pm_prog;
	unsigned pm_vers;
	unsigned pm_prot;
	unsigned pm_port;
};

struct pmaplist {
	pmap	pml_map;
	pmaplist *pml_next;
};

typedef pmaplist *pmaplistp;

program RPCPROG_PMAP { 
	version PMAP_V2  {
		void PMAP_V2_NULL(void) = 0;
		bool PMAP_V2_SET(pmap) = 1;
		bool PMAP_V2_UNSET(pmap) = 2;
		unsigned PMAP_V2_GETPORT(pmap) = 3;
		pmaplistp PMAP_V2_DUMP(void) = 4;
		/* unsigned, string<> PMAP_V2_CALLIT(unsigned, unsigned, unsigned, string<>) = 5; */
	} = 2;
} = 100000;
