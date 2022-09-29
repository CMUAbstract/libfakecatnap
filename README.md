Catnap version be used in Culpeo 2022 evaluation

We lovingly call this code "Fake CatNap" because we had to rewrite CatNap from
scratch to get it to work. Alas, the original code requires quite a bit of
tuning to get it run correctly.

Overall, this library performs the same functions as the original CatNap-- it's
a scheduler that runs events and tasks. Events preempt tasks and tasks only run
if there's sufficient energy to run all events.

We don't include CatNap's feasibility test in this code because we found that
ESR breaks the assumptions CatNap makes on the _charging_ side of things too, so
that's a topic for another paper.

Here's a quick run-down of the different files:

- catnap.c: contains the boot code, the scheduler, and the comparator interrupt
  that does quite a bit of heavy lifting.
- culpeo.c: performs the Culpeo-R logic
- checkpoint.c: handles checkpointing, note that we support a volatile stack,
  unlike CatNap which was optimized for a persistent stack
- fifos: implementation of fifos holding events and tasks
- events.c: handles return from events
- hw.c: couple of function implementations for reading from adc
- scheduler.c: pulled from CatNap
- timers.c: pulled from Catnap
- comp.c: bundle of definitions to use with the MSP430'S comparator, this is
  modeled off of how the CatNap codee did it originally.

A couple more notes:
- LFCN\_<command>'s are ifdefs associated with "LibFakeCatnap
- LCFN\_<command>'s are also ifdefs associated with this library...
- CULPEO\_<command>'s are ifdefs associated with the CULPEO functions in this
  library



