#include "catnap.h"
#include "print_util.h"
#include <stdio.h>
#include <libio/console.h>

volatile __nv uint16_t ticks_waited = 0;
volatile uint16_t ticks_to_wait = 0;

__nv int timer_i = 0;
__nv evt_t buffer = {0};
__nv int buffer_done = 0;

void update_event_timers(uint16_t ticks) {
  for (/*empty*/; timer_i < MAX_EVENTS; timer_i++) {
    if (buffer_done) { //failsafe if we don't finish copy
      if (all_events.events[timer_i] != 0) {
        all_events.events[timer_i]->time_rdy = buffer.time_rdy;
        all_events.events[timer_i]->valid = buffer.valid;
      }
      buffer_done = 0;
      continue;
    }
    evt_t * temp_event;
    temp_event = all_events.events[timer_i];
    if (temp_event != 0 && temp_event->valid != OFF) {
      // Copy values to work on
      buffer.time_rdy = temp_event->time_rdy;
      buffer.valid = temp_event->valid;
      // Reset finished events
      if (buffer.valid == DONE) {
        if (temp_event->periodic == PERIODIC) {
          buffer.time_rdy = temp_event->period;
        }
        else {
          buffer.time_rdy = 0;
        }
        buffer.valid = WAITING; // needs to be after time_rdy update
      }
      else {
        if (temp_event->periodic == PERIODIC) {
          // Now update ticks waited
          buffer.time_rdy -= ticks;
        }
        else {
          buffer.time_rdy = 0;
        }
      }
      // Set ready bits
      if (buffer.time_rdy <= 0) {
        LFCN_DBG_PRINTF("Rdy %u! %x\r\n",timer_i,temp_event);
        buffer.valid = RDY;
      }
      buffer_done = 1;
      all_events.events[timer_i]->time_rdy = buffer.time_rdy;
      all_events.events[timer_i]->valid = buffer.valid;
      buffer_done = 0;
    }
  }
  //PRINTF("ticks:%u\r\n",all_events.events[1]->time_rdy);
  timer_i = 0;
  return;
}

//Idempotent
uint16_t get_next_evt_time(void) {
  uint16_t min_time = 0xffff;
  int16_t temp_time;
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i] == 0) {
      continue;
    }
    if (all_events.events[i]->valid != WAITING) {
      continue;
    }
    temp_time  = all_events.events[i]->time_rdy;
    if (temp_time > 0 && temp_time < min_time) {
      min_time = (uint16_t) temp_time;
    }
  }
  if (min_time == 0xffff) {
    min_time = 0;
  }
  return min_time;
}


