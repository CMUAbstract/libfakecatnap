#ifndef _LIBCATNAP_H_
#define _LIBCATNAP_H_
#include <msp430.h>
#include <libmsp/mem.h>
#include <stdio.h>
#include <stdint.h>

#include "events.h"
#include "tasks.h"



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


extern __nv volatile context_t *curctx;

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


// Operations on arrays of events
int add_event(evt_t *);
int dec_event(evt_t *);
evt_t * pick_event(void);

int push_task(task_t *);
task_t * pop_task(void);

// Timer operations
void update_event_timers(uint16_t);
uint16_t get_next_evt_time(void);

#define STARTER_EVT(func) \
  __nv evt_t EVT_FCN_STARTER = {\
  .evt = &func, \
  .vltg = 0, \
  .time_rdy = 0, \
  .valid = RDY, \
  .period = 0 \
  }

extern evt_t EVT_FCN_STARTER;

extern __nv evt_list_t all_events;
extern __nv task_fifo_t all_tasks;

// Scheduling variables
extern __nv uint16_t ticks_waited;
extern volatile uint16_t ticks_to_wait;


int main(void);
void scheduler(void);

#endif
