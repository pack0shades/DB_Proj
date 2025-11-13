/* slotted_bench.c: load student data into slotted pages and report utilization vs static */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pf.h"
#include "pftypes.h"
#include "slotted.h"

/* Parse one student line into SP_Record, writing strings into provided buffers */
static int parse_student(const char *line, SP_Record *r,
                         char *name_buf, int name_cap,
                         char *dept_buf, int dept_cap,
                         char *level_buf){
    /* fields are semicolon-separated; use: roll_no (2nd), name (3rd), degree token (~13th) */
    char tmp[4096];
    char *tok;
    int idx = 0;
    long roll = 0;

    if (!line || !r || !name_buf || !dept_buf || !level_buf) return -1;

    strncpy(tmp, line, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';

    /* initialize outputs */
    r->name = name_buf; name_buf[0] = '\0';
    r->dept = dept_buf; dept_buf[0] = '\0';
    r->level = level_buf; level_buf[0] = '\0';
    r->roll_no = 0;

    tok = strtok(tmp, ";\r\n");
    while (tok){
        if (idx == 1) {
            roll = strtol(tok, NULL, 10);
        } else if (idx == 2) {
            /* name */
            strncpy(name_buf, tok, name_cap - 1);
            name_buf[name_cap - 1] = '\0';
        } else if (idx == 12) {
            /* degree/department token */
            strncpy(dept_buf, tok, dept_cap - 1);
            dept_buf[dept_cap - 1] = '\0';
        }
        idx++;
        tok = strtok(NULL, ";\r\n");
    }

    r->roll_no = (int32_t)roll;

    /* Derive level from degree token */
    {
        const char *deg = dept_buf;
        int pg = 0;
        if (deg){
            if (strstr(deg, "MSC") || strstr(deg, "ME") || strstr(deg, "MTECH") || strstr(deg, "PHD"))
                pg = 1;
        }
        strcpy(level_buf, pg ? "PG" : "UG");
    }

    return 0;
}

int main(int argc, char **argv){
    const char *in = (argc>1)?argv[1]:"../data/student.txt";
    const char *out = (argc>2)?argv[2]:"students.spf";

    PF_Init();
    PF_SetBufferPoolSize(50);

    /* Optional unit test mode */
    if (getenv("RUN_UNIT")){
        const char *uf = "unit.spf";
        /* ensure clean file */
        PF_DestroyFile((char*)uf);
        if (SP_Create(uf) != PFE_OK) { PF_PrintError("unit SP_Create"); return 1; }
        {
            int ufd = SP_Open(uf);
            if (ufd < 0) { PF_PrintError("unit SP_Open"); return 1; }
            SP_RID rids[50]; int i; char buf[64]; SP_Record rec;
            for (i=0;i<50;i++){
                char name[32]; char dept[8] = "BE"; char lvl[4] = "UG";
                sprintf(name, "S%03d", i);
                rec.roll_no = i; rec.name = name; rec.dept = dept; rec.level = lvl;
                if (SP_Insert(ufd, &rec, &rids[i])!=PFE_OK){ PF_PrintError("unit insert"); break; }
            }
            /* delete evens */
            for (i=0;i<50;i+=2){ int rc = SP_Delete(ufd, rids[i]); if (rc!=PFE_OK){ fprintf(stderr, "unit delete rc=%d at i=%d page=%d slot=%d\n", rc, i, rids[i].page, rids[i].slot); break; } }
            /* scan count */
            {
                SP_Scan sc; SP_Record rr; SP_RID rid; int cnt=0; int rc;
                SP_ScanOpen(ufd, &sc);
                while ((rc=SP_ScanNext(&sc,&rr,&rid,buf,sizeof(buf)))==PFE_OK) cnt++;
                SP_ScanClose(&sc);
                printf("UNIT: expected=25 scanned=%d\n", cnt);
            }
            SP_Close(ufd);
        }
        return 0;
    }

    fprintf(stderr, "slotted_bench: input=%s output=%s\n", in, out);

    /* create slotted file */
    if (SP_Create(out) != PFE_OK) { PF_PrintError("SP_Create"); return 1; }
    {
        int fd = SP_Open(out);
        if (fd < 0) { PF_PrintError("SP_Open"); return 1; }
        /* Prefer MRU for sequential access patterns */
        PF_SetReplPolicy(fd, PF_REPL_MRU);

        FILE *f = fopen(in, "r"); if (!f){ perror("open data"); return 1; }
        char line[4096];
        /* skip header line if any */
        if (!fgets(line, sizeof(line), f)) { fclose(f); return 1; }

        {
            const char *max_env = getenv("MAX_REC");
            const char *del_env = getenv("DEL_EVERY");
            long max_rec = max_env ? atol(max_env) : 0; /* 0 => no limit */
            int del_every = del_env ? atoi(del_env) : 0; /* 0 => no deletes */
            SP_RID *rid_list = NULL; long rid_cap = 0; long rid_cnt = 0;
            long n=0; 
            while (fgets(line, sizeof(line), f)){
                SP_Record rec;
                char name[512];
                char dept[64];
                char lvl[8];
                SP_RID rid;
                if (parse_student(line, &rec, name, sizeof(name), dept, sizeof(dept), lvl) != 0) {
                    fprintf(stderr, "parse error on line %ld\n", n+1);
                    continue;
                }
                if (SP_Insert(fd, &rec, &rid)!=PFE_OK){ PF_PrintError("insert"); break; }
                n++;
                if (del_every>0 && (n % del_every)==0) {
                    if (rid_cnt == rid_cap) {
                        long new_cap = rid_cap? (rid_cap*2) : 1024;
                        SP_RID *tmp = (SP_RID*)realloc(rid_list, new_cap * sizeof(SP_RID));
                        if (!tmp) { fprintf(stderr, "realloc failed at n=%ld\n", n); break; }
                        rid_list = tmp; rid_cap = new_cap;
                    }
                    rid_list[rid_cnt++] = rid;
                }
                if (n%10000==0) { fprintf(stderr, "Inserted %ld\n", n); fflush(stderr); }
                if (max_rec && n>=max_rec) break;
            }
            fclose(f);

            {
                int pages, bytes; SP_Utilization(fd, &pages, &bytes);
                double util = pages? (double)bytes / (pages*PF_PAGE_SIZE) : 0.0;
                printf("Slotted: records=%ld pages=%d bytes=%d util=%.4f\n", n, pages, bytes, util);
                fflush(stdout);

                /* Static comparison at fixed sizes */
                {
                    int sizes[] = {64,128,256,512};
                    int i; for (i=0;i<4;i++){
                        int sz = sizes[i]; long per_page = (PF_PAGE_SIZE / sz);
                        long pages_needed = (n + per_page - 1) / per_page;
                        double s_util = (double)(n * sz) / (pages_needed * PF_PAGE_SIZE);
                        printf("Static(size=%d): pages=%ld util=%.4f\n", sz, pages_needed, s_util);
                    }
                    fflush(stdout);
                }

                /* Optional deletion + compaction test */
                if (rid_cnt > 0) {
                    long i; int rc; int pages2, bytes2; double util2; long kept = n - rid_cnt;
                    fprintf(stderr, "Deleting %ld records (every %dth) ...\n", rid_cnt, del_every);
                    for (i=0;i<rid_cnt;i++) { rc = SP_Delete(fd, rid_list[i]); if (rc!=PFE_OK){ fprintf(stderr, "delete rc=%d at i=%ld (page=%d,slot=%d)\n", rc, i, rid_list[i].page, rid_list[i].slot); break; } }
                    free(rid_list);
                    SP_Utilization(fd, &pages2, &bytes2); util2 = pages2? (double)bytes2 / (pages2*PF_PAGE_SIZE) : 0.0;
                    printf("After delete: kept=%ld pages=%d bytes=%d util=%.4f\n", kept, pages2, bytes2, util2);
                    /* Count via scan */
                    {
                        SP_Scan scan; SP_Record rr; SP_RID rrr; char sbuf[1024]; long cnt=0; int rc2;
                        if ((rc2=SP_ScanOpen(fd, &scan))==PFE_OK){ while ((rc2=SP_ScanNext(&scan,&rr,&rrr,sbuf,sizeof(sbuf)))==PFE_OK) cnt++; SP_ScanClose(&scan); }
                        printf("Scan count after delete: %ld\n", cnt);
                        if (cnt != kept) fprintf(stderr, "WARNING: kept (%ld) != scanned (%ld)\n", kept, cnt);
                    }
                }
            }

            /* Quick scan check: count first 100 records via scan */
            {
                SP_Scan scan; SP_Record r; SP_RID rid; char sbuf[1024]; int i=0; int rc;
                if ((rc=SP_ScanOpen(fd, &scan))!=PFE_OK){ PF_PrintError("scan open"); }
                else {
                    while (i<100 && (rc=SP_ScanNext(&scan, &r, &rid, sbuf, sizeof(sbuf)))==PFE_OK) { i++; }
                    SP_ScanClose(&scan);
                    fprintf(stderr, "Scan checked %d records\n", i);
                }
            }
        }

        SP_Close(fd);
    }
    return 0;
}
