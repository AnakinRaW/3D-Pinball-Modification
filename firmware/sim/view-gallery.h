// The host's views in the Arcade, each filled with made-up data as host-screens.md shows it. A
// press on a control notes its action and leads to the scene the host would show next, so the views
// work as a click-through of the host. Back returns to the scene a page was opened from.
#pragma once
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

/// A scene of the gallery: its key, which the Arcade asks for, and its name in the Arcade.
struct GalleryScene {
    const char* key;
    const char* label;
};

/// Notes a press on a control: the action's name and its value.
using GalleryNote = void (*)(const char* name, int32_t value);

/// Leads to another scene once the current event is done.
using GalleryGo = void (*)(const char* key);

/// Takes the functions the gallery notes presses and leads to scenes with.
void galleryBegin(GalleryNote note, GalleryGo go);

/// Gets the gallery's scenes in the order of host-screens.md.
const GalleryScene* galleryScenes(size_t& count);

/// Builds the scene of @p key into @p parent, a full-screen object on the host's layer.
///
/// @param picked  True for a scene picked in the Arcade, which starts the way back afresh. False
///                for a scene a press led to, which Back returns from.
/// @return        The scene's key as the table holds it, nullptr for a key no scene has.
const char* galleryBuild(const char* key, lv_obj_t* parent, bool picked);

/// Leads to a scene from the scene shown, which Back then returns to, as the host's menu opens its
/// pages.
void galleryOpen(const char* key);

/// Gets whether a scene stands over a running game, where the swipe opens the host's menu.
bool galleryInGame(const char* key);

/// Stops what the scene shown runs, such as a countdown, before its objects go.
void galleryLeave();
