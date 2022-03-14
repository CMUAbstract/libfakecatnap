#ifndef _LIBFAKECATNAP_EVENTS_H_
#define _LIBFAKECATNAP_EVENTS_H_

typedef enum evt_valid_ {
  OFF,
  WAITING,
  RDY,
  DONE
} evt_valid_t;

typedef void (evt_func_t)(void);

typedef struct evt_ {
  // Func pointer
  evt_func_t *evt;
  // Starting voltage, may need to be a table
  uint16_t vltg;
  int16_t time_rdy;
  evt_valid_t valid;
  uint16_t period;
} evt_t;

#define EVENT(name) EVT_ ## name


#define DEC_EVT(name, func, per)\
  evt_t EVT_ ## name  =  \
  { .evt = &func, \
    .vltg = 0, \
    .period = per, \
    .time_rdy = per, \
    .valid = WAITING, \
  }

int event_return();

#define EVENT_RETURN() \
  event_return();

#endif
