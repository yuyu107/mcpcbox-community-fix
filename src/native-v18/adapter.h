#ifndef MCPC_ADAPTER_H
#define MCPC_ADAPTER_H
#include <stdint.h>
typedef struct {unsigned long long bytes;uint32_t type,id;int isGroup;wchar_t name[512],path[1024];} HistoryRow;
typedef struct {uint32_t type,id;int ok;char error[512];} HistoryMutation;
int adapter_history_remove_batch(HistoryMutation *entries,int count,char *err);
unsigned long adapter_history_revision(void);
int adapter_history_target(uint32_t type,uint32_t id,wchar_t *out,int *grouped,char *err);
int adapter_history_list(HistoryRow **rows,int *count,char *err);
int adapter_history_remove(uint32_t type,uint32_t id,char *err);
int adapter_history_path(uint32_t type,uint32_t id,wchar_t *out,char *err);
void adapter_init(void);
void adapter_seed(unsigned long seed);
int adapter_root(wchar_t *out,int capacity,char *err);
int adapter_submit(const wchar_t *name,const wchar_t *url,const wchar_t *target,void **task,char *err);
int adapter_status(void *task,unsigned long long *bytes,int *error);
int adapter_task_pending(void *task);
int adapter_group_sync(void);
void adapter_cancel(void *task);
void adapter_wait(void *task,unsigned long timeout);
void adapter_release(void *task);
int adapter_register(const wchar_t *gamePath,char *err);
int adapter_unregister(const wchar_t *gamePath,char *err);
int adapter_group_begin(const wchar_t *name,const wchar_t *directory,unsigned long long total,char *err);
void adapter_group_progress(unsigned long long done);
void adapter_group_finish(int state);
int adapter_group_paused(void);
extern void *resource_resume,*resource_seh,*success_resume,*error_resume;
void AdapterResourceGate(void);
void AdapterSuccessGate(void);
void AdapterErrorGate(void);
#endif
