/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _PMAP_H_RPCGEN
#define _PMAP_H_RPCGEN

#include <rpc/rpc.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PMAPPORT 111

struct __attribute__((packed)) pmap {
	u_int pm_prog;
	u_int pm_vers;
	u_int pm_prot;
	u_int pm_port;
};
typedef struct pmap pmap;

struct __attribute__((packed)) pmaplist {
	pmap pml_map;
	struct pmaplist *pml_next;
};
typedef struct pmaplist pmaplist;

typedef pmaplist *pmaplistp;

#define RPCPROG_PMAP 100000
#define PMAP_V2 2

#if defined(__STDC__) || defined(__cplusplus)
#define PMAP_V2_NULL 0
extern  enum clnt_stat pmap_v2_null_2(void *, void *, CLIENT *);
extern  bool_t pmap_v2_null_2_svc(void *, void *, struct svc_req *);
#define PMAP_V2_SET 1
extern  enum clnt_stat pmap_v2_set_2(pmap *, bool_t *, CLIENT *);
extern  bool_t pmap_v2_set_2_svc(pmap *, bool_t *, struct svc_req *);
#define PMAP_V2_UNSET 2
extern  enum clnt_stat pmap_v2_unset_2(pmap *, bool_t *, CLIENT *);
extern  bool_t pmap_v2_unset_2_svc(pmap *, bool_t *, struct svc_req *);
#define PMAP_V2_GETPORT 3
extern  enum clnt_stat pmap_v2_getport_2(pmap *, u_int *, CLIENT *);
extern  bool_t pmap_v2_getport_2_svc(pmap *, u_int *, struct svc_req *);
#define PMAP_V2_DUMP 4
extern  enum clnt_stat pmap_v2_dump_2(void *, pmaplistp *, CLIENT *);
extern  bool_t pmap_v2_dump_2_svc(void *, pmaplistp *, struct svc_req *);
extern int rpcprog_pmap_2_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define PMAP_V2_NULL 0
extern  enum clnt_stat pmap_v2_null_2();
extern  bool_t pmap_v2_null_2_svc();
#define PMAP_V2_SET 1
extern  enum clnt_stat pmap_v2_set_2();
extern  bool_t pmap_v2_set_2_svc();
#define PMAP_V2_UNSET 2
extern  enum clnt_stat pmap_v2_unset_2();
extern  bool_t pmap_v2_unset_2_svc();
#define PMAP_V2_GETPORT 3
extern  enum clnt_stat pmap_v2_getport_2();
extern  bool_t pmap_v2_getport_2_svc();
#define PMAP_V2_DUMP 4
extern  enum clnt_stat pmap_v2_dump_2();
extern  bool_t pmap_v2_dump_2_svc();
extern int rpcprog_pmap_2_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_pmap (XDR *, pmap*);
extern  bool_t xdr_pmaplist (XDR *, pmaplist*);
extern  bool_t xdr_pmaplistp (XDR *, pmaplistp*);

#else /* K&R C */
extern bool_t xdr_pmap ();
extern bool_t xdr_pmaplist ();
extern bool_t xdr_pmaplistp ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_PMAP_H_RPCGEN */
