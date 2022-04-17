#include "catnap.h"
#include <stdio.h>
#include <libio/console.h>
#include "culpeo.h"

__nv volatile fifo_meta_t fifo_0 = {0,0,0};
__nv volatile fifo_meta_t fifo_1 = {0,0,0};

// Adds event to global event list
int add_event(evt_t *event) {
  // Confirm that event doesn't already exist-- handles repeated writes to the
  // event array.
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i] == event){
      //  Event is already present, set it to ready
      event->valid = WAITING;
      return 1;
    }
  }
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i] == 0) {
      PRINTF("New evt!\r\n");
      all_events.events[i] = event;
      break;
    }
    if ( i == MAX_EVENTS - 1) {
      PRINTF("no room\r\n");
      return 1;  // No room
    }
  }
  return 0;
}

// Pulls event out of global event list
int dec_event(evt_t *event) {
  event->valid = OFF; // This is idempotent :-D
  return 0;
}


evt_t * pick_event() {
  evt_t * next_e = NULL;
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i] == 0) {
      continue;
    }
    if (all_events.events[i]->valid == STARTED) {
      //PRINTF("Got started!!!!\r\n");
      next_e = all_events.events[i];
      return next_e;
    }
    if (all_events.events[i]->valid == RDY) {
      next_e = all_events.events[i];
      //return all_events.events[i];
    }
  }
  return next_e;
}

void dump_events() {
  PRINTF("Event dump:\r\n");
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i] == 0) {
      continue;
    }
    PRINTF("%x ",all_events.events[i]);
    print_float(all_events.events[i]->V_min);
    print_float(all_events.events[i]->V_final);
    PRINTF("\r\n");
  }
  PRINTF("\r\n");
  return;
}


// Push task on global task fifo
int push_task(task_t *task) {
  if (curctx->fifo->tsk_cnt >= MAX_TASKS){
    PRINTF("no task room!\r\n");
    return 1; //No room in fifo
  }
  fifo_meta_t *next_fifo = curctx->fifo == &fifo_0 ? &fifo_1 : &fifo_0;
  if (curctx->fifo->back < MAX_TASKS - 1) {
    all_tasks.tasks[curctx->fifo->back] = task;
    next_fifo->back = curctx->fifo->back + 1;
  }
  else {
    all_tasks.tasks[0] = task;
    next_fifo->back = curctx->fifo->back + 0;
  }
  next_fifo->tsk_cnt = curctx->fifo->tsk_cnt + 1;
  curctx->fifo = next_fifo;
  PRINTF("task slotted\r\n");
  return 0;
}

// Returns task at front of fifo
task_t * pop_task() {
  if (curctx->fifo->tsk_cnt == 0) {
    PRINTF("no tasks!\r\n");
    return NULL; // no tasks left
  }
  PRINTF("New task!\r\n");
  fifo_meta_t *next_fifo = curctx->fifo == &fifo_0 ? &fifo_1 : &fifo_0;
  next_fifo->tsk_cnt = curctx->fifo->tsk_cnt - 1; 
  uint8_t old_front = curctx->fifo->front;
  if (curctx->fifo->front < MAX_TASKS - 1) {
    next_fifo->front = curctx->fifo->front + 1;
  }
  else {
    next_fifo->front = 0;
  }
  return all_tasks.tasks[old_front];
}

// Commits changes to the fifo made in other functions
// We assume that changes were written to the fifo not in use in curctx
void update_task_fifo(context_t * ctx) {
  fifo_meta_t *next_fifo = curctx->fifo == &fifo_0 ? &fifo_1 : &fifo_0;
  ctx->fifo = next_fifo; // update is here
  return;
}
