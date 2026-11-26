#include "notice-view.h"

namespace {
constexpr int32_t INSET_PX = 12;         // the card's texts and buttons from its sides
constexpr int32_t TEXT_TOP_PX = 10;      // the first line below the strip
constexpr int32_t LINE_GAP_PX = 10;      // between two lines, and before the buttons
constexpr int32_t BULB_GAP_PX = 8;       // between a bulb and its texts
constexpr int32_t BULB_DROP_PX = 3;      // a bulb below its line's top
constexpr int32_t BUTTON_GAP_PX = 8;     // between two buttons
constexpr int32_t WHOLE_PERCENT = 100;   // the card's whole width
constexpr int32_t HALF = 2;
constexpr int32_t TEXT_ROOM_PX = 16;   // a button's rim and the room inside it, both sides

/// Creates a container without LVGL's theme, as wide as its parent, that grows with its content
/// and stacks it in @p flow.
lv_obj_t* stack(lv_obj_t* parent, lv_flex_flow_t flow, int32_t gap) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, lv_pct(WHOLE_PERCENT), LV_SIZE_CONTENT);
    lv_obj_set_scrollable(o, false);
    lv_obj_set_clickable(o, false);
    lv_obj_set_flex_flow(o, flow);
    lv_obj_set_style_pad_row(o, gap, 0);
    lv_obj_set_style_pad_column(o, gap, 0);
    return o;
}

/// Creates a label that wraps at its container's width.
lv_obj_t* text(lv_obj_t* parent, const char* t, const lv_font_t* font, uint32_t rgb) {
    lv_obj_t* l = controls::label(parent, t, font, rgb);
    lv_obj_set_width(l, lv_pct(WHOLE_PERCENT));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_recolor(l, true);
    return l;
}

/// Sets a button's text a size smaller where it would reach the plaque's rim, as the storyboard
/// does for KEEP PLAYING.
void fit(lv_obj_t* button) {
    lv_obj_t* l = lv_obj_get_child(button, 0);
    lv_point_t size;
    lv_text_get_size(&size, lv_label_get_text(l), &archivo_black_14, 0, 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    if (size.x > lv_obj_get_style_width(button, LV_PART_MAIN) - TEXT_ROOM_PX) {
        lv_obj_set_style_text_font(button, &archivo_black_12, 0);
    }
}
}   // namespace

void NoticeView::build(lv_obj_t* parent, const Notice& notice, Pressed pressed, void* context) {
    lv_obj_t* root = create(parent, true, pressed, context);
    lv_obj_t* card = nullptr;
    if (notice.modal) {
        card = controls::dialog(root, notice.title, notice.width, LV_SIZE_CONTENT, notice.tone);
    } else {
        controls::rays(root, true);
        card = controls::card(root, notice.tone, notice.title, notice.width, LV_SIZE_CONTENT);
        lv_obj_center(card);
    }
    // The body stands below the strip and holds the lines and then the buttons.
    lv_obj_t* body = stack(card, LV_FLEX_FLOW_COLUMN, LINE_GAP_PX);
    lv_obj_set_y(body, controls::STRIP_PX);
    lv_obj_set_style_pad_hor(body, INSET_PX, 0);
    lv_obj_set_style_pad_top(body, TEXT_TOP_PX, 0);
    lv_obj_set_style_pad_bottom(body, INSET_PX, 0);
    for (uint8_t i = 0; i < notice.count && i < LINES_MAX; ++i) {
        const Line& line = notice.lines[i];
        lv_obj_t* row = stack(body, LV_FLEX_FLOW_ROW, BULB_GAP_PX);
        // A bulb's glow reaches past the line.
        lv_obj_set_overflow_visible(row, true);
        if (line.bulb) {
            lv_obj_t* b = controls::bulb(row, line.light);
            lv_obj_set_style_margin_top(b, BULB_DROP_PX, 0);
        }
        lv_obj_t* texts = stack(row, LV_FLEX_FLOW_COLUMN, 0);
        lv_obj_set_flex_grow(texts, 1);
        text(texts, line.title, &montserrat_bold_14, controls::color::CREAM);
        if (line.detail[0] != '\0') {
            text(texts, line.detail, &montserrat_semibold_12, controls::color::MUTED);
        }
    }
    lv_obj_t* buttons = stack(body, LV_FLEX_FLOW_ROW, BUTTON_GAP_PX);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    const int32_t width =
        (notice.width - INSET_PX * HALF - BUTTON_GAP_PX - controls::GAP_PX * HALF) / HALF;
    if (notice.secondary != nullptr) {
        fit(controls::button(buttons, controls::Plaque::VIOLET, notice.secondary, width));
        bind(lv_obj_get_child(buttons, -1), SECONDARY);
    }
    fit(controls::button(buttons, notice.primaryPlaque, notice.primary, width));
    bind(lv_obj_get_child(buttons, -1), PRIMARY);
}
