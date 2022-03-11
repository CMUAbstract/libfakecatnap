#ifndef _TASKS_H_
#define _TASKS_H_

typedef struct task_ {
  task_func_t *func;
} task_t;

#define TASK(name) TSK_ ## name


#define DEC_TSK(name, func)\
  task_t TSK_ ## name  =  \
  { .func = &func; \
  }
  

#endif

