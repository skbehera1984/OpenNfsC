xdrs=nfs mount nlm sm pmap nfs4
includedir=$(PROJECT_SOURCE_DIR)/include/nfsrpc
xdr_sources = $(foreach type,xdr,$(addsuffix _$(type).c,$(xdrs)))
xdr_headers = $(addprefix $(includedir)/,$(addsuffix .h,$(xdrs)))
SOURCES = $(xdr_sources)
LOCAL_SOURCES = $(xdr_headers)
CLEAN_FILES += $(xdr_sources) $(xdr_headers)

$(includedir)/%.h: %.x
    mkdir -p $(dir $@) ;\
    rm -f $@ ; rpcgen -CM -h $< | sed 's/^\([   ]*struct \)\(.*{\)$$/\1__attribute__((packed)) \2/' > $@
# we pack all the structs because many match the network format and we use them directly
# really, we only need to pack those without pointers, but the performance loss shouldn't be very noticable since everything's 4-byte anyway
%_xdr.c: %.x
    rm -f $@ ; rpcgen -CM -c -o $@ $<
%_clnt.c: %.x
    rm -f $@ ; rpcgen -CM -l -o $@ $<
%_svr.c: %.x
    rm -f $@ ; rpcgen -CM -m -o $@ $<
