// freertos-teensy 11.2.0-4 with its pull request 43, which sets the heap's start before any
// constructor runs; these four values replace the port's own. The port's FreeRTOSConfig.h includes
// this file last, under the name freertos_config_override.h, once the build's include path holds it
#pragma once

#undef  configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 13         // the port's 2; the kernel masks only priority numbers 208 and up
#undef  configUSE_CUSTOM_YIELD_HANDLER
#define configUSE_CUSTOM_YIELD_HANDLER              1           // the port's 0; the storage driver defines yield() and calls the port's default_yield() outside the storage task
#undef  configTEENSY_HEAP_ALLOCATION
#define configTEENSY_HEAP_ALLOCATION                2           // the port's 1; malloc() stays in RAM2, where PJRC's core puts it
#undef  configMAIN_STACK_DEPTH
#define configMAIN_STACK_DEPTH                      ( 8192U )   // the port's 4096; the interrupts' stack at the top of RAM1
