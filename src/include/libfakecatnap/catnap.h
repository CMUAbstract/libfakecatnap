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


#define MAX_EVENTS 10
#define MAX_TASKS 10

typedef struct evt_list_ {
  evt_t *events[MAX_EVENTS]; //TODO does fixed size work?
  uint8_t cur_event; // pointer to cur event
} evt_list_t;

typedef struct task_fifo_ {
  task_t *tasks[MAX_TASKS];
} task_fifo_t;


typedef struct fifo_meta_ {
  uint8_t tsk_cnt;
  uint8_t front;
  uint8_t back;
} fifo_meta_t;

typedef struct context_ {
  task_t *active_task; //points to any active task we (for instance) interrupted
  evt_t *active_evt; // points to an event currently running
  activity_t mode; // defines what we're actually doing
  uint8_t pwr_lvl; // incoming power level for feasibility estimates
  fifo_meta_t *fifo; // pointer to data for task fifo so we can double buffer it
} context_t;

extern __nv volatile context_t *curctx;
extern __nv volatile fifo_meta_t fifo_0;
extern __nv volatile fifo_meta_t fifo_1;

// Operations on arrays of events
int add_event(evt_t *);
int dec_event(evt_t *);
evt_t * pick_event(void);

int push_task(task_t *);
task_t * pop_task(void);
void update_task_fifo(context_t *ctx);

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
