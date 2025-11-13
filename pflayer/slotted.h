#ifndef SLOTTED_H
#define SLOTTED_H

/* RID to identify a record */
typedef struct {
    int page;
    int slot;
} SP_RID;

/* Variable-length student record */
typedef struct {
    long roll_no;   /* 32-bit fits in long here */
    const char *name;   /* bytes */
    const char *dept;   /* degree/department code */
    const char *level;  /* "UG" or "PG" */
} SP_Record;

/* Opaque scan handle */
typedef struct {
    int fd;         /* PF file descriptor */
    int page;       /* current page */
    int slot;       /* current slot index */
} SP_Scan;

/* File operations */
int SP_Create(const char *fname);
int SP_Open(const char *fname);
int SP_Close(int fd);

/* Record operations */
int SP_Insert(int fd, const SP_Record *rec, SP_RID *rid_out);
int SP_Get(int fd, SP_RID rid, SP_Record *rec_out, char *buf, int bufcap);
int SP_Delete(int fd, SP_RID rid);

/* Scan operations */
int SP_ScanOpen(int fd, SP_Scan *scan);
int SP_ScanNext(SP_Scan *scan, SP_Record *rec_out, SP_RID *rid_out, char *buf, int bufcap);
int SP_ScanClose(SP_Scan *scan);

/* Utilization */
int SP_Utilization(int fd, int *pages_out, int *bytes_used_out);

#endif /* SLOTTED_H */
