#include "catnap.h"
#include <stdio.h>
#include <libio/console.h>

__nv uint16_t ticks_waited = 0;
volatile uint16_t ticks_to_wait = 0;

__nv int timer_i = 0;
__nv evt_t buffer = {0};
__nv int buffer_done = 0;

void update_event_timers(uint16_t ticks) {
  for (/*empty*/; timer_i < MAX_EVENTS; timer_i++) {
    if (buffer_done) { //failsafe if we don't finish copy
      all_events.events[timer_i]->time_rdy = buffer.time_rdy;
      all_events.events[timer_i]->valid = buffer.valid;
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
        buffer.time_rdy = temp_event->period;
        buffer.valid = WAITING; // needs to be after time_rdy update
      }
      else {
        // Now update ticks waited
        buffer.time_rdy -= ticks; //TODO need shadow
      }
      // Set ready bits
      if (buffer.time_rdy <= 0) {
        buffer.valid = RDY;
      }
      buffer_done = 1;
      all_events.events[timer_i]->time_rdy = buffer.time_rdy;
      all_events.events[timer_i]->valid = buffer.valid;
      buffer_done = 0;
    }
  }
  timer_i = 0;
  return;
}

//Idempotent
uint16_t get_next_evt_time(void) {
  uint16_t min_time = 0x7fff;
  int16_t temp_time;
  for (int i = 0; i < MAX_EVENTS; i++) {
    if (all_events.events[i]->valid != WAITING) {
      continue;
    }
    temp_time  = all_events.events[i]->time_rdy;
    if (temp_time > 0 && temp_time < min_time) {
      min_time = (uint16_t) temp_time;
    }
  }
  return min_time;
}


