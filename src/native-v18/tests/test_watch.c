#include "../transfer_watch.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 TransferWatch w={0};
 assert(!(transfer_watch(&w,0,44999,0)&WATCH_RETRY));
 assert(transfer_watch(&w,0,1,0)&WATCH_RETRY);
 assert(!(transfer_watch(&w,10,1,0)&WATCH_RETRY));
 assert(w.idle==0);
 for(int i=0;i<1000;i++)assert(!(transfer_watch(&w,10,120,1)&WATCH_RETRY));
 assert(w.idle==0&&w.probe==0);
 assert(transfer_watch(&w,10,2000,0)&WATCH_VERIFY);
 assert(!(transfer_watch(&w,11,44000,0)&WATCH_RETRY));
 assert(w.idle==0);
 assert(transfer_watch(&w,11,45000,0)&WATCH_RETRY);
 puts("PASS stalled-transfer deadline, progress reset, file-verification interval and pause exemption.");
}

