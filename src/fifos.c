#include "catnap.h"
#include <stdio.h>
#include <libio/console.h>

// Adds event to global event list
int add_event(evt_t *event) {
  //PRINTF("Adding! %x\r\n",event->evt);
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i] == 0) {
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

// TODO, could make this more robust and check that it's actually scheduled
// Pulls event out of global event list
int dec_event(evt_t *event) {
  event->valid = OFF;
  return 0;
}


evt_t * pick_event() {
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i]->valid == RDY) {
      return all_events.events[i];
    }
  }
  return NULL;
}



// Push task on global task fifo
int push_task(task_t *task) {
  if (all_tasks.tsk_cnt >= MAX_TASKS){
    PRINTF("no task room!\r\n");
    return 1; //No room in fifo
  }
  if (all_tasks.back < MAX_TASKS - 1) {
    PRINTF("Push!\r\n");
    all_tasks.tasks[all_tasks.back] = task;
    all_tasks.back++;
  }
  else {
    all_tasks.tasks[0] = task;
    all_tasks.back = 0;
  }
  all_tasks.tsk_cnt++; //TODO double check that this kind of committing works
  PRINTF("Task count is: %u\r\n",all_tasks.tsk_cnt);
  return 0;
}

// Returns task at front of fifo
// TODO double buffer this nonsense
task_t * pop_task() {
  if (all_tasks.tsk_cnt == 0) {
    PRINTF("no tasks!\r\n");
    return NULL; // no tasks left
  }
  PRINTF("Getting task!\r\n");
  all_tasks.tsk_cnt--;
  uint8_t old_front = all_tasks.front;
  if (all_tasks.front < MAX_TASKS - 1) {
    all_tasks.front++;
  }
  else {
    all_tasks.front = 0;
  }
  return all_tasks.tasks[old_front];
}


