#include "view.h"
#include <stdint.h>

namespace {
constexpr int32_t WHOLE_PERCENT = 100;   // the parent's whole size
constexpr uint32_t ACTION_BITS = 8;      // a control's user data: the value above the action
constexpr uintptr_t ACTION_MASK = 0xFF;
}   // namespace

void View::score(uint32_t value, char (&out)[SCORE_CHARS]) {
    constexpr uint32_t BASE = 10;
    constexpr uint8_t GROUP = 3;
    char reversed[SCORE_CHARS] = {};
    size_t n = 0;
    uint8_t digits = 0;
    do {
        if (digits == GROUP) {
            reversed[n++] = ',';
            digits = 0;
        }
        reversed[n++] = char('0' + value % BASE);
        value /= BASE;
        ++digits;
    } while (value != 0);
    for (size_t i = 0; i < n; ++i) {
        out[i] = reversed[n - 1 - i];
    }
    out[n] = '\0';
}

void View::close() {
    if (root_ != nullptr) {
        lv_obj_t* r = root_;
        root_ = nullptr;
        lv_obj_delete(r);
    }
}

lv_obj_t* View::create(lv_obj_t* parent, bool page, Pressed pressed, void* context) {
    close();
    pressed_ = pressed;
    context_ = context;
    for (lv_obj_t*& t : tracks_) {
        t = nullptr;
    }
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, lv_pct(WHOLE_PERCENT), lv_pct(WHOLE_PERCENT));
    lv_obj_set_scrollable(root_, false);
    lv_obj_set_clickable(root_, page);
    // A parent deleted with the view inside it leaves the view closed.
    lv_obj_add_event_cb(root_, deleted, LV_EVENT_DELETE, this);
    return root_;
}

void View::bind(lv_obj_t* control, uint8_t action, int32_t value) {
    const uintptr_t packed = uintptr_t(uint32_t(value)) << ACTION_BITS | action;
    lv_obj_set_user_data(control, reinterpret_cast<void*>(packed));
    lv_obj_add_event_cb(control, clicked, LV_EVENT_CLICKED, this);
}

void View::bindValue(lv_obj_t* track, uint8_t action) {
    for (uint8_t i = 0; i < TRACKS_MAX; ++i) {
        if (tracks_[i] == nullptr) {
            tracks_[i] = track;
            trackActions_[i] = action;
            lv_obj_add_event_cb(track, changed, LV_EVENT_VALUE_CHANGED, this);
            return;
        }
    }
}

void View::report(uint8_t action, int32_t value) const {
    if (pressed_ != nullptr) {
        pressed_(action, value, context_);
    }
}

void View::clicked(lv_event_t* e) {
    const auto* view = static_cast<const View*>(lv_event_get_user_data(e));
    lv_obj_t* control = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const auto packed = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(control));
    view->report(uint8_t(packed & ACTION_MASK), int32_t(uint32_t(packed >> ACTION_BITS)));
}

void View::changed(lv_event_t* e) {
    const auto* view = static_cast<const View*>(lv_event_get_user_data(e));
    lv_obj_t* track = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    for (uint8_t i = 0; i < TRACKS_MAX; ++i) {
        if (view->tracks_[i] == track) {
            view->report(view->trackActions_[i], lv_slider_get_value(track));
        }
    }
}

void View::deleted(lv_event_t* e) {
    auto* view = static_cast<View*>(lv_event_get_user_data(e));
    if (lv_event_get_current_target(e) == view->root_) {
        view->root_ = nullptr;
    }
}
