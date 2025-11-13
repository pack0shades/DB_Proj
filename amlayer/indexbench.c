#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "am.h"
#include "pf.h"
#include "testam.h"
#include "../pflayer/slotted.h"

/* Missing AM prototypes in legacy headers */
int AM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength);
int AM_DestroyIndex(char *fileName, int indexNo);
int AM_InsertEntry(int fileDesc, char attrType, int attrLength, char *value, int recId);
int AM_OpenIndexScan(int fileDesc, char attrType, int attrLength, int op, char *value);
int AM_FindNextEntry(int scanDesc);
int AM_CloseIndexScan(int scanDesc);
void AM_PrintError(char *s);

/* Missing PF prototypes in legacy amlayer/pf.h */
int PF_OpenFile(char *fname);
int PF_CloseFile(int fd);
int PF_SetReplPolicy(int fd, int policy);
#ifndef PF_REPL_LRU
#define PF_REPL_LRU 0
#define PF_REPL_MRU 1
#endif

/* Forward declarations for PF stats from PF layer (not in AM's pf.h) */
typedef struct PFStats {
    long logical_reads, logical_writes, physical_reads, physical_writes, buffer_hits, buffer_misses;
} PFStats;
extern void PF_StatsReset();
extern void PF_StatsGet(PFStats *out);
extern int PF_SetBufferPoolSize(int n);

static inline int pack_rid(int page, int slot){ return ((page & 0xFFFF) << 16) | (slot & 0xFFFF); }
static inline unsigned long now_us(){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (unsigned long)ts.tv_sec*1000000ul + (unsigned long)(ts.tv_nsec/1000); }

typedef struct { int key; int rid; } Pair;

static int cmp_pair(const void *a, const void *b){ const Pair *pa=(const Pair*)a, *pb=(const Pair*)b; return (pa->key>pb->key)-(pa->key<pb->key); }

static void stats_get(PFStats *st){ PF_StatsGet(st); }

static void print_stats_line(const char *mode, const char *op, long param, long n, const PFStats *st, double ms, FILE *csv){
    if (csv){
        fprintf(csv, "%s,%s,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%.3f\n", mode, op, param, n,
            st->logical_reads, st->logical_writes, st->physical_reads, st->physical_writes, st->buffer_hits, st->buffer_misses, ms);
        fflush(csv);
    } else {
        printf("mode=%s op=%s param=%ld n=%ld lr=%ld lw=%ld pr=%ld pw=%ld hit=%ld miss=%ld ms=%.3f\n",
            mode, op, param, n, st->logical_reads, st->logical_writes, st->physical_reads, st->physical_writes, st->buffer_hits, st->buffer_misses, ms);
    }
}

static FILE* open_csv(const char *path, int write_header){
    if (!path) return NULL;
    int exists = (access(path, F_OK) == 0);
    const char *mode = (!write_header && exists) ? "a" : "w";
    FILE *f = fopen(path, mode);
    if (!f){ fprintf(stderr, "cannot open CSV '%s': %s\n", path, strerror(errno)); return NULL; }
    if (mode[0]=='w' && write_header){ fprintf(f, "mode,op,param,n,lr,lw,pr,pw,hit,miss,ms\n"); fflush(f);} 
    return f;
}

static void minmax_keys(Pair *pairs, long n, int *mink, int *maxk){
    if (n<=0){ *mink=0; *maxk=0; return; }
    int mn=pairs[0].key, mx=pairs[0].key; long i; for (i=1;i<n;i++){ if (pairs[i].key<mn) mn=pairs[i].key; if (pairs[i].key>mx) mx=pairs[i].key; } *mink=mn; *maxk=mx;
}

int main(int argc, char **argv){
    const char *spfile = (argc>1)? argv[1] : "../pflayer/students.spf";
    const char *idxbase = (argc>2)? argv[2] : "student";
    int mode = (argc>3)? atoi(argv[3]) : 0; /* 0=incremental, 1=sorted */
    const char *max_env = getenv("MAX_REC"); long max_rec = max_env? atol(max_env) : 0;
    const char *csv_path = getenv("CSV_OUT"); int csv_header = getenv("CSV_HEADER")? 1:0;
    int qnum = getenv("QNUM")? atoi(getenv("QNUM")) : 100;
    int rnum = getenv("RNUM")? atoi(getenv("RNUM")) : 50;      /* number of range queries */
    int range_pct = getenv("RANGEPCT")? atoi(getenv("RANGEPCT")) : 10; /* percent of domain per range */
    const char *pol = getenv("POLICY"); /* LRU or MRU for index fd */

    PF_Init();
    const char *bufs = getenv("TOYDB_PF_BUFS");
    if (!bufs){ PF_SetBufferPoolSize(50); }

    int spfd = SP_Open(spfile); if (spfd < 0){ PF_PrintError(); return 1; }

    /* gather pairs */
    Pair *pairs = 0; long cap=0, n=0; SP_Scan scan; SP_Record r; SP_RID rid; char buf[1024]; int rc;
    SP_ScanOpen(spfd, &scan);
    while ((rc=SP_ScanNext(&scan, &r, &rid, buf, sizeof(buf))) == PFE_OK){
        if (cap==n){ long nc = cap? cap*2 : 8192; void *tmp = realloc(pairs, nc*sizeof(Pair)); if (!tmp){ fprintf(stderr,"oom\n"); return 1;} pairs=(Pair*)tmp; cap=nc; }
        pairs[n].key = (int)r.roll_no; pairs[n].rid = pack_rid(rid.page, rid.slot); n++;
        if (max_rec && n>=max_rec) break;
    }
    SP_ScanClose(&scan);
    printf("Loaded %ld records from %s\n", n, spfile);

    FILE *csv = open_csv(csv_path, csv_header);

    /* prepare index name and create */
    AM_DestroyIndex((char*)idxbase, 0); /* ignore errors if not exists */
    AM_CreateIndex((char*)idxbase, 0, INT_TYPE, sizeof(int));
    {
        char iname[128]; sprintf(iname, "%s.0", idxbase);
        int ifd = PF_OpenFile(iname); if (ifd < 0){ PF_PrintError(); return 1; }
        if (pol && (pol[0]=='M' || pol[0]=='m')) PF_SetReplPolicy(ifd, PF_REPL_MRU); else PF_SetReplPolicy(ifd, PF_REPL_LRU);

        /* build */
        if (mode==1){ qsort(pairs, (unsigned long)n, (unsigned long)sizeof(Pair), cmp_pair); }
        PF_StatsReset(); unsigned long t0 = now_us();
        {
            long i; for (i=0;i<n;i++){ int key = pairs[i].key; int recid = pairs[i].rid; int err = AM_InsertEntry(ifd, INT_TYPE, sizeof(int), (char*)&key, recid); if (err!=AME_OK){ AM_PrintError("insert"); break; } }
        }
        double ms = (now_us()-t0)/1000.0; PFStats st; stats_get(&st);
        print_stats_line(mode? "build_sorted" : "build_incremental", "build", 0, n, &st, ms, csv);

        /* ALL-scan sanity */
        PF_StatsReset(); t0 = now_us();
        { int hits=0; int sd = AM_OpenIndexScan(ifd, INT_TYPE, sizeof(int), EQ_OP, NULL); int rec; while ((rec=AM_FindNextEntry(sd))>=0) hits++; AM_CloseIndexScan(sd); }
        ms = (now_us()-t0)/1000.0; stats_get(&st); print_stats_line(mode? "build_sorted" : "build_incremental", "scan_all", 0, n, &st, ms, csv);

        /* Point queries over sample */
        if (n>0 && qnum>0){
            int m = (qnum < n)? qnum : (int)n; int i;
            long tot_lr=0,tot_lw=0,tot_pr=0,tot_pw=0,tot_hit=0,tot_miss=0; double tot_ms=0.0; int found=0;
            srand(12345);
            for (i=0;i<m;i++){
                int idx = (int)((rand()/(double)RAND_MAX) * (n-1)); if (idx<0) idx=0; if (idx>=(int)n) idx=(int)n-1;
                int key = pairs[idx].key;
                PF_StatsReset(); t0 = now_us();
                { int sd = AM_OpenIndexScan(ifd, INT_TYPE, sizeof(int), EQ_OP, (char*)&key); int rec; while ((rec=AM_FindNextEntry(sd))>=0){ found++; } AM_CloseIndexScan(sd); }
                ms = (now_us()-t0)/1000.0; stats_get(&st);
                tot_lr+=st.logical_reads; tot_lw+=st.logical_writes; tot_pr+=st.physical_reads; tot_pw+=st.physical_writes; tot_hit+=st.buffer_hits; tot_miss+=st.buffer_misses; tot_ms+=ms;
            }
            PFStats avg={ tot_lr/m, tot_lw/m, tot_pr/m, tot_pw/m, tot_hit/m, tot_miss/m };
            print_stats_line(mode? "build_sorted" : "build_incremental", "point_eq", m, n, &avg, tot_ms/m, csv);
        }

        /* Range queries (RANGEPCT of [min,max]) */
        if (n>0 && rnum>0 && range_pct>0){
            int mn, mx; minmax_keys(pairs, n, &mn, &mx); int domain = (mx - mn + 1); if (domain<=0) domain = n; 
            int width = (range_pct * domain) / 100; if (width<1) width=1;
            int i; long tot_lr=0,tot_lw=0,tot_pr=0,tot_pw=0,tot_hit=0,tot_miss=0; double tot_ms=0.0; long total_hits=0;
            srand(9876);
            for (i=0;i<rnum;i++){
                int start = mn + (rand() % domain);
                int end = start + width;
                PF_StatsReset(); unsigned long t0 = now_us();
                { int sd = AM_OpenIndexScan(ifd, INT_TYPE, sizeof(int), GE_OP, (char*)&start); int rec; while ((rec=AM_FindNextEntry(sd))>=0){ if (rec > end) break; total_hits++; } AM_CloseIndexScan(sd); }
                double ms2 = (now_us()-t0)/1000.0; PFStats st2; stats_get(&st2);
                tot_lr+=st2.logical_reads; tot_lw+=st2.logical_writes; tot_pr+=st2.physical_reads; tot_pw+=st2.physical_writes; tot_hit+=st2.buffer_hits; tot_miss+=st2.buffer_misses; tot_ms+=ms2;
            }
            PFStats avg={ tot_lr/rnum, tot_lw/rnum, tot_pr/rnum, tot_pw/rnum, tot_hit/rnum, tot_miss/rnum };
            print_stats_line(mode? "build_sorted" : "build_incremental", "range_ge_le", width, total_hits/rnum, &avg, tot_ms/rnum, csv);
        }

        PF_CloseFile(ifd);
    }

    SP_Close(spfd);
    if (pairs) free(pairs);
    if (csv) fclose(csv);
    return 0;
}
