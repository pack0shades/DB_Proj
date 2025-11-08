/* buf.c: buffer management routines. The interface routines are:
PFbufGet(), PFbufUnfix(), PFbufAlloc(), PFbufReleaseFile(), PFbufUsed() and
PFbufPrint() */
#include <stdio.h>
#include <stdlib.h>
#include "pf.h"
#include "pftypes.h"

extern int PF_max_bufs; /* runtime-configurable pool size */
extern int PF_GetReplPolicy();
extern void PF_StatsBufferHit();
extern void PF_StatsBufferMiss();

static int PFnumbpage = 0; /* # of buffer pages in memory */
static PFbpage *PFfirstbpage= NULL; /* ptr to first buffer page, or NULL */
static PFbpage *PFlastbpage = NULL; /* ptr to last buffer page, or NULL */
static PFbpage *PFfreebpage= NULL; /* list of free buffer pages */

/* extern char *malloc(); */

static void PFbufInsertFree(bpage)
PFbpage *bpage; {
	/* Insert at head of free list */
	bpage->nextpage = PFfreebpage;
	bpage->prevpage = NULL;
	PFfreebpage = bpage;
}


static void PFbufLinkHead(bpage)
PFbpage *bpage; {
	/* link at head of used list */
	bpage->nextpage = PFfirstbpage;
	bpage->prevpage = NULL;
	if (PFfirstbpage != NULL)
		PFfirstbpage->prevpage = bpage;
	PFfirstbpage = bpage;
	if (PFlastbpage == NULL)
		PFlastbpage = bpage;
}

static void PFbufLinkTail(bpage)
PFbpage *bpage; {
	/* link at tail of used list */
	bpage->nextpage = NULL;
	bpage->prevpage = PFlastbpage;
	if (PFlastbpage != NULL)
		PFlastbpage->nextpage = bpage;
	PFlastbpage = bpage;
	if (PFfirstbpage == NULL)
		PFfirstbpage = bpage;
}
	
void PFbufUnlink(bpage)
PFbpage *bpage; {
	if (bpage==NULL) return;
	if (PFfirstbpage == bpage)
		PFfirstbpage = bpage->nextpage;
	if (PFlastbpage == bpage)
		PFlastbpage = bpage->prevpage;
	if (bpage->nextpage != NULL)
		bpage->nextpage->prevpage = bpage->prevpage;
	if (bpage->prevpage != NULL)
		bpage->prevpage->nextpage = bpage->nextpage;
	bpage->prevpage = bpage->nextpage = NULL;
}


static PFbufInternalAlloc(bpage,writefcn)
PFbpage **bpage; int (*writefcn)(); {
PFbpage *tbpage; int error;
	/* choose from free list */
	if (PFfreebpage != NULL){
		*bpage = PFfreebpage;
		PFfreebpage = PFfreebpage->nextpage;
	}
	else if (PFnumbpage < PF_max_bufs){
		if ((*bpage=(PFbpage *)malloc(sizeof(PFbpage)))==NULL){
			*bpage = NULL; PFerrno = PFE_NOMEM; return(PFerrno);
		}
		PFnumbpage++;
	}
	else {
		/* pick victim from tail (global order list) */
		*bpage = NULL;
		for (tbpage=PFlastbpage; tbpage!=NULL; tbpage=tbpage->prevpage){
			if (!tbpage->fixed) break; /* found victim */
		}
		if (tbpage == NULL){ PFerrno = PFE_NOBUF; return(PFerrno); }
		/* write victim if dirty */
		if (tbpage->dirty && (error=(*writefcn)(tbpage->fd,tbpage->page,&tbpage->fpage))!=PFE_OK)
			return(error);
		tbpage->dirty = FALSE;
		/* remove from hash */
		if ((error=PFhashDelete(tbpage->fd,tbpage->page))!=PFE_OK)
			return(error);
		PFbufUnlink(tbpage);
		*bpage = tbpage;
	}
	/* link allocated page at head initially (caller may move) */
	PFbufLinkHead(*bpage);
	return(PFE_OK);
}

/************************* Interface to the Outside World ****************/

PFbufGet(fd,pagenum,fpage,readfcn,writefcn)
int fd; int pagenum; PFfpage **fpage; int (*readfcn)(); int (*writefcn)(); {
PFbpage *bpage; int error; int policy;
	policy = PF_GetReplPolicy(fd);
	if ((bpage=PFhashFind(fd,pagenum)) == NULL){
		/* miss */
		if ((error=PFbufInternalAlloc(&bpage,writefcn))!= PFE_OK){ *fpage=NULL; return(error);} 
		/* read from disk */
		if ((error=(*readfcn)(fd,pagenum,&bpage->fpage))!= PFE_OK){ PFbufUnlink(bpage); PFbufInsertFree(bpage); *fpage=NULL; return(error);} 
		if ((error=PFhashInsert(fd,pagenum,bpage))!=PFE_OK){ PFbufUnlink(bpage); PFbufInsertFree(bpage); return(error);} 
		bpage->fd = fd; bpage->page = pagenum; bpage->dirty = FALSE; bpage->fixed = TRUE; 
		/* reorder according to policy: LRU head already; MRU -> tail */
		if (policy==PF_REPL_MRU){ PFbufUnlink(bpage); PFbufLinkTail(bpage);} 
		PF_StatsBufferMiss();
	}
	else if (bpage->fixed){
		/* already fixed */
		*fpage = &bpage->fpage; PFerrno = PFE_PAGEFIXED; return(PFerrno);
	}
	else {
		/* hit */
		PF_StatsBufferHit();
		bpage->fixed = TRUE;
		/* move according to policy: LRU->head, MRU->tail */
		PFbufUnlink(bpage);
		if (policy==PF_REPL_MRU) PFbufLinkTail(bpage); else PFbufLinkHead(bpage);
	}
	*fpage = &bpage->fpage; return(PFE_OK);
}

PFbufUnfix(fd,pagenum,dirty)
int fd; int pagenum; int dirty; {
PFbpage *bpage; int policy; 
	if ((bpage= PFhashFind(fd,pagenum))==NULL){ PFerrno = PFE_PAGENOTINBUF; return(PFerrno);} 
	if (!bpage->fixed){ PFerrno = PFE_PAGEUNFIXED; return(PFerrno);} 
	if (dirty) bpage->dirty = TRUE; bpage->fixed = FALSE; policy = PF_GetReplPolicy(fd); 
	/* reorder: LRU -> head (most recently used), MRU -> tail (recent goes out sooner) */
	PFbufUnlink(bpage);
	if (policy==PF_REPL_MRU) PFbufLinkTail(bpage); else PFbufLinkHead(bpage); 
	return(PFE_OK);
}

PFbufAlloc(fd,pagenum,fpage,writefcn)
int fd; int pagenum; PFfpage **fpage; int (*writefcn)(); {
PFbpage *bpage; int error; int policy = PF_GetReplPolicy(fd);
	*fpage = NULL;
	if ((bpage=PFhashFind(fd,pagenum))!= NULL){ PFerrno = PFE_PAGEINBUF; return(PFerrno);} 
	if ((error=PFbufInternalAlloc(&bpage,writefcn))!= PFE_OK) return(error);
	if ((error=PFhashInsert(fd,pagenum,bpage))!= PFE_OK){ PFbufUnlink(bpage); PFbufInsertFree(bpage); return(error);} 
	bpage->fd = fd; bpage->page = pagenum; bpage->fixed = TRUE; bpage->dirty = FALSE; 
	if (policy==PF_REPL_MRU){ PFbufUnlink(bpage); PFbufLinkTail(bpage);} 
	*fpage = &bpage->fpage; return(PFE_OK);
}

PFbufReleaseFile(fd,writefcn)
int fd; int (*writefcn)(); {
PFbpage *bpage; PFbpage *temppage; int error; 
	bpage = PFfirstbpage;
	while (bpage != NULL){
		if (bpage->fd == fd){
			if (bpage->fixed){ PFerrno = PFE_PAGEFIXED; return(PFerrno);} 
			if (bpage->dirty && (error=(*writefcn)(fd,bpage->page,&bpage->fpage))!=PFE_OK) return(error);
			bpage->dirty = FALSE;
			if ((error=PFhashDelete(fd,bpage->page))!=PFE_OK){ printf("Internal error:PFbufReleaseFile()\n"); exit(1);} 
			temppage = bpage; bpage = bpage->nextpage; PFbufUnlink(temppage); PFbufInsertFree(temppage);
		}
		else bpage = bpage->nextpage;
	}
	return(PFE_OK);
}

PFbufUsed(fd,pagenum)
int fd; int pagenum; {
PFbpage *bpage; int policy = PF_GetReplPolicy(fd);
	if ((bpage=PFhashFind(fd,pagenum))==NULL){ PFerrno = PFE_PAGENOTINBUF; return(PFerrno);} 
	if (!(bpage->fixed)){ PFerrno = PFE_PAGEUNFIXED; return(PFerrno);} 
	bpage->dirty = TRUE; PFbufUnlink(bpage); if (policy==PF_REPL_MRU) PFbufLinkTail(bpage); else PFbufLinkHead(bpage); return(PFE_OK);
}

void PFbufPrint(){
PFbpage *bpage; printf("buffer content:\n"); if (PFfirstbpage == NULL) printf("empty\n"); else { printf("fd\tpage\tfixed\tdirty\tfpage\n"); for(bpage = PFfirstbpage; bpage != NULL; bpage= bpage->nextpage) printf("%d\t%d\t%d\t%d\t%d\n", bpage->fd,bpage->page,(int)bpage->fixed,(int)bpage->dirty,(int)&bpage->fpage); }
}
