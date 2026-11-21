// Sets the FreeRTOS values in which the firmware differs from the freertos-teensy port's defaults.
// The port's FreeRTOSConfig.h includes this file last, so a value here replaces the port's default.
// The file takes effect only where build_flags puts this folder on the include path.
#pragma once

// The port's 2. The kernel masks only priority numbers 208 and up.
#undef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 13

// The port's 0. The storage driver defines yield() and calls the port's default_yield() outside the
// storage task.
#undef configUSE_CUSTOM_YIELD_HANDLER
#define configUSE_CUSTOM_YIELD_HANDLER 1

// The port's 0. The storage task and the main task share a priority and take turns every tick, so
// a card that holds SdFat in a wait never stops the main loop.
#undef configUSE_TIME_SLICING
#define configUSE_TIME_SLICING 1

// The port's 1. malloc() stays in RAM2, where PJRC's core puts it.
#undef configTEENSY_HEAP_ALLOCATION
#define configTEENSY_HEAP_ALLOCATION 2

// The port's 4096. The interrupts' stack at the top of RAM1.
#undef configMAIN_STACK_DEPTH
#define configMAIN_STACK_DEPTH (8192U)
