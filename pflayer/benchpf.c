/* benchpf.c: microbenchmark to generate PF stats under configurable read/write mix */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pf.h"
#include "pftypes.h"

#define DBFILE "benchpf.dat"

static void ensure_file(int pages){
    int fd, i, pagenum; char *buf;
    if (PF_CreateFile(DBFILE) == PFE_OK) {
        fd = PF_OpenFile(DBFILE);
        for (i=0;i<pages;i++){ if (PF_AllocPage(fd,&pagenum,&buf)!=PFE_OK){ PF_PrintError("alloc"); exit(1);} memset(buf,0,PF_PAGE_SIZE); if (PF_UnfixPage(fd,pagenum,1)!=PFE_OK){ PF_PrintError("unfix"); exit(1);} }
        PF_CloseFile(fd);
    }
}

int main(int argc, char **argv){
    int pool = (argc>1)?atoi(argv[1]):20; /* pool size */
    int total_ops = (argc>2)?atoi(argv[2]):1000; /* total ops */
    int write_pct = (argc>3)?atoi(argv[3]):10; /* percent writes */
    int policy = (argc>4)?atoi(argv[4]):PF_REPL_LRU; /* 0 LRU, 1 MRU */
    int npages = (argc>5)?atoi(argv[5]):100; /* working set pages */

    PF_Init();
    if (PF_SetBufferPoolSize(pool)!=PFE_OK) { PF_PrintError("set pool"); return 1; }
    ensure_file(npages);

    int fd = PF_OpenFile(DBFILE);
    if (fd<0){ PF_PrintError("open"); return 1; }
    PF_SetReplPolicy(fd, policy);
    PF_StatsReset();

    int i; for (i=0;i<total_ops;i++){
        int p = rand()%npages; char *buf; int rc;
        if ((rand()%100) < write_pct){
            /* write */
            rc = PF_GetThisPage(fd,p,&buf);
            if (rc==PFE_OK){ buf[0]++; PF_MarkDirty(fd,p); PF_UnfixPage(fd,p,1); }
        } else {
            /* read */
            rc = PF_GetThisPage(fd,p,&buf);
            if (rc==PFE_OK){ volatile char c = buf[0]; (void)c; PF_UnfixPage(fd,p,0); }
        }
    }

    PFStats st; PF_StatsGet(&st);
    const char *outfile = (argc>6)?argv[6]:"pf_stats.csv";
    PF_StatsWrite(outfile);
    printf("Wrote stats to %s (lr=%ld lw=%ld pr=%ld pw=%ld hit=%ld miss=%ld)\n", outfile, st.logical_reads, st.logical_writes, st.physical_reads, st.physical_writes, st.buffer_hits, st.buffer_misses);

    PF_CloseFile(fd);
    return 0;
}
