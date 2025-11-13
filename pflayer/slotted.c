#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pf.h"
#include "pftypes.h"
#include "slotted.h"

#define SP_MAGIC 0x53504631u /* 'SPF1' */

typedef struct {
    unsigned long magic;
    unsigned short free_off;
    unsigned short free_bytes;
    unsigned short nslots;
    unsigned short _pad;
} SP_PageHdr;

typedef struct {
    unsigned short off;
    unsigned short len;
} SP_Slot;

#define SP_HDR_SIZE ((int)sizeof(SP_PageHdr))
#define SP_SLOT_SIZE ((int)sizeof(SP_Slot))

static SP_PageHdr *sp_hdr(char *pagebuf){ return (SP_PageHdr*)pagebuf; }
static SP_Slot *sp_slot(char *pagebuf, int idx){
    /* fixed slot address independent of nslots */
    return (SP_Slot*)(pagebuf + PF_PAGE_SIZE - (idx + 1) * SP_SLOT_SIZE);
}

static void sp_init_page(char *pagebuf){
    SP_PageHdr *h = sp_hdr(pagebuf);
    h->magic = SP_MAGIC;
    h->free_off = (unsigned short)SP_HDR_SIZE;
    h->nslots = 0;
    h->free_bytes = (unsigned short)(PF_PAGE_SIZE - SP_HDR_SIZE);
    h->_pad = 0;
}

static int sp_ensure_slot(char *pagebuf){
    SP_PageHdr *h = sp_hdr(pagebuf);
    int i;
    for (i=0;i<h->nslots;i++){
        SP_Slot *s = sp_slot(pagebuf, i);
        if (s->len == 0) return i;
    }
    if (h->free_bytes < SP_SLOT_SIZE) return -1;
    h->free_bytes -= SP_SLOT_SIZE;
    /* zero-initialize the new slot */
    {
        SP_Slot *ns = sp_slot(pagebuf, h->nslots);
        ns->off = 0; ns->len = 0;
    }
    h->nslots++;
    return h->nslots - 1;
}

static void sp_compact(char *pagebuf){
    SP_PageHdr *h = sp_hdr(pagebuf);
    int i; unsigned short off = (unsigned short)SP_HDR_SIZE;
    for (i=0;i<h->nslots;i++){
        SP_Slot *s = sp_slot(pagebuf, i);
        if (s->len == 0) continue;
        if (s->off != off){ memmove(pagebuf + off, pagebuf + s->off, s->len); s->off = off; }
        off += s->len;
    }
    h->free_off = off;
    h->free_bytes = (unsigned short)(PF_PAGE_SIZE - off - h->nslots*SP_SLOT_SIZE);
}

static int sp_serialize(const SP_Record *r, char *dst, int cap){
    int nlen = r->name ? (int)strlen(r->name) : 0;
    int dlen = r->dept ? (int)strlen(r->dept) : 0;
    int llen = r->level ? (int)strlen(r->level) : 0;
    int need = 4 + 2 + nlen + 2 + dlen + 2 + llen;
    if (dst==NULL) return need;
    if (need > cap) return -1;
    {
        int off = 0;
        long rn = r->roll_no; memcpy(dst+off, &rn, 4); off+=4;
        {
            unsigned short t;
            t = (unsigned short)nlen; memcpy(dst+off,&t,2); off+=2; if (nlen){ memcpy(dst+off,r->name,nlen); off+=nlen; }
            t = (unsigned short)dlen; memcpy(dst+off,&t,2); off+=2; if (dlen){ memcpy(dst+off,r->dept,dlen); off+=dlen; }
            t = (unsigned short)llen; memcpy(dst+off,&t,2); off+=2; if (llen){ memcpy(dst+off,r->level,llen); off+=llen; }
        }
    }
    return need;
}

static int sp_deserialize(const char *src, int len, SP_Record *out, char *buf, int cap){
    unsigned short nlen, dlen, llen; int off;
    if (len < 4+2+2+2) return -1;
    off=0; memcpy(&out->roll_no, src+off, 4); off+=4;
    memcpy(&nlen, src+off, 2); off+=2; if (nlen+1 > cap) return -1; memcpy(buf, src+off, nlen); buf[nlen]='\0'; out->name = buf; off+=nlen; buf+=nlen+1; cap-=nlen+1;
    memcpy(&dlen, src+off, 2); off+=2; if (dlen+1 > cap) return -1; memcpy(buf, src+off, dlen); buf[dlen]='\0'; out->dept = buf; off+=dlen; buf+=dlen+1; cap-=dlen+1;
    memcpy(&llen, src+off, 2); off+=2; if (llen+1 > cap) return -1; memcpy(buf, src+off, llen); buf[llen]='\0'; out->level = buf; (void)off;
    return 0;
}

int SP_Create(const char *fname){ return PF_CreateFile((char*)fname); }
int SP_Open(const char *fname){ int fd = PF_OpenFile((char*)fname); return fd; }
int SP_Close(int fd){ return PF_CloseFile(fd); }

static int sp_find_page(int fd, int need_bytes){
    int rc, pno; char *pbuf;
    rc = PF_GetFirstPage(fd, &pno, &pbuf);
    while (rc == PFE_OK){
        SP_PageHdr *h = sp_hdr(pbuf);
        if (h->magic != SP_MAGIC){ sp_init_page(pbuf); if (PF_UnfixPage(fd, pno, TRUE) != PFE_OK) return -1; return pno; }
        if (h->free_bytes >= (unsigned short)need_bytes){ PF_UnfixPage(fd, pno, FALSE); return pno; }
        PF_UnfixPage(fd, pno, FALSE);
        rc = PF_GetNextPage(fd, &pno, &pbuf);
    }
    return -1;
}

int SP_Insert(int fd, const SP_Record *rec, SP_RID *rid_out){
    int rlen = sp_serialize(rec, NULL, 0);
    int need; int pno; char *pbuf; int rc; SP_PageHdr *h; int slot; SP_Slot *s; char *dst;
    if (rlen <= 0 || rlen > PF_PAGE_SIZE - SP_HDR_SIZE - SP_SLOT_SIZE) return PFE_NOBUF;
    need = rlen + SP_SLOT_SIZE;
    pno = sp_find_page(fd, need);
    if (pno < 0){ rc = PF_AllocPage(fd, &pno, &pbuf); if (rc != PFE_OK) return rc; sp_init_page(pbuf); }
    else { rc = PF_GetThisPage(fd, pno, &pbuf); if (rc != PFE_OK) return rc; }
    h = sp_hdr(pbuf); if (h->magic != SP_MAGIC){ sp_init_page(pbuf); }
    slot = sp_ensure_slot(pbuf);
    if (slot < 0){ sp_compact(pbuf); slot = sp_ensure_slot(pbuf); if (slot < 0){ PF_UnfixPage(fd, pno, FALSE); return PFE_NOBUF; } }
    if (h->free_bytes < (unsigned short)rlen){ sp_compact(pbuf); if (h->free_bytes < (unsigned short)rlen){ PF_UnfixPage(fd, pno, FALSE); return PFE_NOBUF; } }
    dst = pbuf + h->free_off; sp_serialize(rec, dst, h->free_bytes);
    s = sp_slot(pbuf, slot); s->off = h->free_off; s->len = (unsigned short)rlen; h->free_off += (unsigned short)rlen; h->free_bytes -= (unsigned short)rlen;
    if (rid_out){ rid_out->page = pno; rid_out->slot = slot; }
    return PF_UnfixPage(fd, pno, TRUE);
}

int SP_Get(int fd, SP_RID rid, SP_Record *rec_out, char *buf, int bufcap){
    char *pbuf; int rc = PF_GetThisPage(fd, rid.page, &pbuf); SP_PageHdr *h; SP_Slot *s; int ret;
    if (rc!=PFE_OK) return rc; h = sp_hdr(pbuf); if (h->magic != SP_MAGIC || rid.slot<0 || rid.slot>=h->nslots){ PF_UnfixPage(fd,rid.page,FALSE); return PFE_INVALIDPAGE; }
    s = sp_slot(pbuf, rid.slot); if (s->len==0){ PF_UnfixPage(fd,rid.page,FALSE); return PFE_PAGEFREE; }
    ret = sp_deserialize(pbuf + s->off, s->len, rec_out, buf, bufcap); PF_UnfixPage(fd, rid.page, FALSE); return (ret==0)?PFE_OK:PFE_NOBUF;
}

int SP_Delete(int fd, SP_RID rid){
    char *pbuf; int rc = PF_GetThisPage(fd, rid.page, &pbuf); SP_PageHdr *h; SP_Slot *s;
    if (rc!=PFE_OK) return rc; h = sp_hdr(pbuf); if (h->magic != SP_MAGIC || rid.slot<0 || rid.slot>=h->nslots){ PF_UnfixPage(fd,rid.page,FALSE); return PFE_INVALIDPAGE; }
    s = sp_slot(pbuf, rid.slot); if (s->len==0){ PF_UnfixPage(fd,rid.page,FALSE); return PFE_PAGEFREE; }
    s->len = 0; s->off = 0; sp_compact(pbuf); return PF_UnfixPage(fd, rid.page, TRUE);
}

int SP_ScanOpen(int fd, SP_Scan *scan){ scan->fd=fd; scan->page=-1; scan->slot=-1; return PFE_OK; }
int SP_ScanNext(SP_Scan *scan, SP_Record *rec_out, SP_RID *rid_out, char *buf, int bufcap){
    int fd = scan->fd; char *pbuf; int pno; int rc; SP_PageHdr *h; int start, i;
    if (scan->page < 0) rc = PF_GetFirstPage(fd, &pno, &pbuf); else { pno = scan->page; rc = PF_GetThisPage(fd, pno, &pbuf); }
    while (rc == PFE_OK){
        h = sp_hdr(pbuf);
        if (h->magic != SP_MAGIC){
            PF_UnfixPage(fd,pno,FALSE);
            rc = PF_GetNextPage(fd, &pno, &pbuf);
            continue;
        }
        start = (scan->page==pno)? (scan->slot+1) : 0;
        for (i=start;i<h->nslots;i++){
            SP_Slot *s = sp_slot(pbuf, i);
            if (s->len==0) continue;
            if (sp_deserialize(pbuf + s->off, s->len, rec_out, buf, bufcap)==0){ if (rid_out){ rid_out->page=pno; rid_out->slot=i; } scan->page = pno; scan->slot = i; PF_UnfixPage(fd, pno, FALSE); return PFE_OK; }
        }
        PF_UnfixPage(fd, pno, FALSE); rc = PF_GetNextPage(fd, &pno, &pbuf); scan->slot = -1;
    }
    return PFE_EOF;
}
int SP_ScanClose(SP_Scan *scan){ scan->fd=-1; scan->page=-1; scan->slot=-1; return PFE_OK; }

int SP_Utilization(int fd, int *pages_out, int *bytes_used_out){
    int rc, pno, pages=0, bytes=0; char *pbuf; SP_PageHdr *h; int i;
    rc = PF_GetFirstPage(fd, &pno, &pbuf);
    while (rc == PFE_OK){ h = sp_hdr(pbuf); if (h->magic == SP_MAGIC){ pages++; for (i=0;i<h->nslots;i++){ SP_Slot *s = sp_slot(pbuf,i); bytes += s->len; } } PF_UnfixPage(fd,pno,FALSE); rc = PF_GetNextPage(fd,&pno,&pbuf); }
    if (pages_out) *pages_out = pages; if (bytes_used_out) *bytes_used_out = bytes; return PFE_OK;
}
