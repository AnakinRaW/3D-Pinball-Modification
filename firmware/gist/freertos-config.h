// freertos-teensy 11.2.0-4 with its pull request 43, which sets the heap's start before any
// constructor runs; these four values replace the port's own

#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 13         // the port's 2; the kernel masks only priority numbers 208 and up
#define configUSE_CUSTOM_YIELD_HANDLER              1           // the port's 0; the storage driver defines yield()
#define configTEENSY_HEAP_ALLOCATION                2           // the port's 1; malloc() stays in RAM2, where PJRC's core puts it
#define configMAIN_STACK_DEPTH                      ( 8192U )   // the port's 4096; the interrupts' stack at the top of RAM1
