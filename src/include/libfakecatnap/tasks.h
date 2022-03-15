#ifndef _LIBFAKECATNAP_TASKS_H_
#define _LIBFAKECATNAP_TASKS_H_

typedef void (task_func_t)(void);

typedef struct task_ {
  task_func_t *func;
  uint8_t valid_chkpt;
} task_t;

#define TASK(name) TSK_ ## name


#define DEC_TSK(name, f)\
  task_t TSK_ ## name  =  \
  { .func = &f, \
    .valid_chkpt = 0\
  }
  

#endif

