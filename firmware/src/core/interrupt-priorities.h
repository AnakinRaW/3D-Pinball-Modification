// Checks that every interrupt the firmware enables runs at the priority its row of the interrupt
// table gives. FreeRTOS sets every interrupt to one priority when it starts. The Teensy core and
// the Audio library set priorities as well, so a changed library can move an interrupt without a
// line of the firmware changing. The check finds that at the start and writes it to the log.
//
// Design: docs/firmware/general-design.md, Interrupts.
#pragma once
#include <Arduino.h>

/// Compares the priority of every enabled interrupt of the interrupt table with its row.
///
/// An interrupt that is not enabled is left out, since its driver has not begun. Where priorities
/// differ, one warning goes to the log with the count and the first interrupt that differs.
///
/// @return  How many interrupts differ from their row, 0 where every one matches.
/// @note    The main task calls it once, after the start and before the main loop.
uint32_t checkInterruptPriorities();
