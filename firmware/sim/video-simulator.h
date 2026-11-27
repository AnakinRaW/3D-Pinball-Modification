// The video player in the Arcade's WebAssembly module: the firmware's video player, file system,
// storage service and sounds over a memory card that the browser fills with files. The browser
// hands each file over once, and the card reads from it frame by frame, so a long film stays in
// the browser's memory and not in the module's.
#pragma once
#include "components/file-system.h"
#include "components/screen.h"
#include "components/video-player.h"

/// Starts the storage, the file system, the sounds and the video player on @p screen.
///
/// A second call does nothing.
///
/// @note  The screen's begin() comes first.
void videoBegin(Screen& screen);

/// Serves the card's reads and runs the video player's update, as the storage task and the main
/// loop do on the machine.
///
/// @note  Every pass calls it, before the screen renders.
void videoPass();

/// Gets the file system the video player reads through.
FileSystem& videoFiles();

/// Gets the video player.
///
/// @return  Null before videoBegin().
VideoPlayer* videoPlayer();
