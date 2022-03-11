#ifndef _CATNAP_H_
#define _CATNAP_H_

#include "events.h"
#include "tasks.h"

typedef void (task_func_t)(void);
typedef void (evt_func_t)(void);


typedef enum activity_ {
  CHARGING,
  SLEEPING,
  EVENT,
  TASK
} activity_t;

typedef struct context_ {
  task_t *active_task;
  evt_t *active_evt;
  activity_t mode;
  uint8_t pwr_lvl;
} context_t;

#define MAX_EVENTS 10
#define MAX_TASKS 10

typedef struct evt_list_ {
  evt_t *events[MAX_EVENTS]; //TODO does fixed size work?
  uint8_t cur_event; // pointer to cur event
} evt_list_t;

typedef struct task_fifo_ {
  task_t *tasks[MAX_TASKS];
  uint8_t tsk_cnt;
  uint8_t front;
  uint8_t back;
} task_fifo_t;


__nv evt_list_t all_events = {.events = {0}, cur_event = MAX_EVENTS+1}; 
__nv task_fifo_t all_tasks = {.tasks = {0}, .tsk_cnt = 0, .front = 0, .back = 0};

context_t context_0 = {0};
context_t context_1 = {0};
context_t *curctx = &context_0;

int add_event(evt_t *);
int dec_event(evt_t *);

int push_task(task_t *);
task_t * pop_task(void);


#define STARTER_EVT(func) \
  EVT_FCN_STARTER = { \
  .evt = &func; \
  .vltg = 0; \
  .time_rdy = 0; \
  .valid = RDY; \
  }

extern evt_t EVT_FCN_STARTER;

#endif
