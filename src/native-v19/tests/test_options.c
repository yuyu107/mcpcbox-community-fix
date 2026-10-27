#include "../request_options.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(download_parallel("id=1.8.9")==8);
 assert(download_parallel("id=1.8.9&parallel=4")==4);
 assert(download_parallel("id=1.8.9&parallel=8")==8);
 assert(download_parallel("id=1.8.9&parallel=16")==16);
 assert(download_parallel("id=1.8.9&parallel=999999999999999999999999")==8);
 assert(download_parallel("id=1.8.9&parallel=16evil")==8);
 assert(download_parallel("id=1.8.9&notparallel=16")==8);
 assert(download_parallel("id=1.8.9&parallel=-1")==8);
 puts("PASS concurrency request parsing: defaults, valid choices, invalid and excessive values.");
}

