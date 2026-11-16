// The firmware's four FreeRTOS settings for freertos-teensy 11.2.0-4 with its pull request 43,
// which sets the heap's start before any constructor runs. These four values replace the port's
// own. The port's FreeRTOSConfig.h includes this file last once the include path holds it. The
// firmware and the test image put this folder there with build_flags, which reach the libraries.
#pragma once

// The port's 2. The kernel masks only priority numbers 208 and up.
#undef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 13

// The port's 0. The storage driver defines yield() and calls the port's default_yield() outside the
// storage task.
#undef configUSE_CUSTOM_YIELD_HANDLER
#define configUSE_CUSTOM_YIELD_HANDLER 1

// The port's 1. malloc() stays in RAM2, where PJRC's core puts it.
#undef configTEENSY_HEAP_ALLOCATION
#define configTEENSY_HEAP_ALLOCATION 2

// The port's 4096. The interrupts' stack at the top of RAM1.
#undef configMAIN_STACK_DEPTH
#define configMAIN_STACK_DEPTH (8192U)
