#ifndef _EVENTS_H_
#define _EVENTS_H_

typedef enum evt_valid_ {
  OFF,
  WAITING,
  RDY
} evt_valid_t;


typedef struct evt_ {
  // Func pointer
  evt_func_t *evt;
  // Starting voltage, may need to be a table
  uint16_t vltg;
  uint16_t time_rdy;
  evt_valid_t valid;
} evt_t;

#define EVENT(name) EVT_ ## name


#define DEC_EVT(name, func, period)\
  evt_t EVT_ ## name  =  \
  { .evt = &func; \
    .vltg = 0; \
    .time_rdy = period; \
    .valid = OFF; \
  }
  
#endif
