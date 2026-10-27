#ifndef MCPC_TRANSFER_WATCH_H
#define MCPC_TRANSFER_WATCH_H
#include <stdint.h>
typedef struct {uint64_t bytes;uint32_t idle,probe;} TransferWatch;
enum { WATCH_VERIFY=1,WATCH_RETRY=2 };
static unsigned transfer_watch(TransferWatch *w,uint64_t bytes,uint32_t elapsed,int paused){
 unsigned flags=0;
 if(paused||bytes!=w->bytes)w->idle=0;
 else if(elapsed>=45000-w->idle)w->idle=45000;
 else w->idle+=elapsed;
 w->bytes=bytes;
 if(!paused){if(elapsed>=2000-w->probe){w->probe=0;flags|=WATCH_VERIFY;}else w->probe+=elapsed;}
 else w->probe=0;
 if(w->idle>=45000)flags|=WATCH_RETRY;
 return flags;
}
#endif

