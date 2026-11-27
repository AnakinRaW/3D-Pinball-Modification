// The screen in the Arcade's WebAssembly module: the firmware's screen component and touch driver
// on the simulated display and touch controller. The browser draws the display's pixels and
// reports where a finger or the mouse touches the screen.
#pragma once

/// Runs the screen's part of one pass of the main loop: the driver tick when it is due, the touch
/// controller's answer, the touch events into the screen component, and LVGL.
///
/// @note  sim/simulator.cpp calls it in every pass, after it has set the clock. Before the
///        browser's screenBegin() it does nothing.
void screenPass();
