#include "catnap.h"
#include <stdio.h>
#include <libio/console.h>

__nv uint16_t ticks_waited = 0;
volatile uint16_t ticks_to_wait = 0;

void update_event_timers(uint16_t ticks) {
  for (int i = 0; i < MAX_EVENTS; i++) {
    evt_t * temp_event;
    temp_event = all_events.events[i];
    if (temp_event != 0 && temp_event->valid != OFF) {
      // Reset finished events
      if (temp_event->valid == DONE) {
        temp_event->time_rdy = temp_event->period;
        temp_event->valid = WAITING; // needs to be after time_rdy update
      }
      else {
        // Now update ticks waited
        temp_event->time_rdy -= ticks; //TODO need shadow
      }
      // Set ready bits
      if (temp_event->time_rdy <= 0) {
        temp_event->valid = RDY;
      }
    }
  }
}

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


