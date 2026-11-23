#include "sdkconfig.h"

#if CONFIG_AI_PASSPORT_KIDS_UI

#include "kids_display.h"

#include <esp_log.h>
#include <esp_random.h>
#include <material_symbols.h>
#include <src/misc/lv_text_private.h>

#include <cctype>
#include <cstring>
#include <initializer_list>

#include "application.h"
#include "assets/lang_config.h"
#include "kids_strings.h"
#include "lvgl_theme.h"

#define TAG "KidsDisplay"

namespace {

// Palette (dark text on the pastel pill, light text on the night sky).
constexpr uint32_t kText = 0xFFF6FB;
constexpr uint32_t kTextSoft = 0xD4CAF5;
constexpr uint32_t kDarkText = 0x2A1846;
constexpr uint32_t kBubbleFill = 0x261E56;
constexpr uint32_t kPopupFill = 0x3A1C4E;
constexpr uint32_t kPink = 0xFF96C4;
constexpr uint32_t kLilac = 0xB89EFF;
constexpr uint32_t kMint = 0x78FFD6;
constexpr uint32_t kYellow = 0xFFDE68;
constexpr uint32_t kCoral = 0xFF8C7A;
constexpr uint32_t kDim = 0x5A5294;

// Layout of the 240x320 screen. Its corners have a 30 px radius, so content
// stays 12 px from the edges and out of the 30x30 corner squares.
constexpr int kIconY = 16;
constexpr int kIconInset = 30;
constexpr int kPillY = 11;
constexpr int kPillH = 30;
constexpr int kPillPadX = 12;
constexpr int kPillPadTop = 2;
constexpr int kPillTextMax = 108;      // pill spans x 54..186 at most
constexpr int kPillTextMaxMuted = 64;  // leaves room for the mute icon
constexpr int kBubbleX = 12;
constexpr int kBubbleW = 216;
constexpr int kBubbleBottom = 308;
constexpr int kBubbleRadius = 18;  // concentric with the screen corner
constexpr int kBorder = 2;
constexpr int kPadX = 12;
constexpr int kPadTop = 12;
constexpr int kPadBottom = 10;
constexpr int kTextW = kBubbleW - 2 * (kBorder + kPadX);
constexpr int kLineSpace = -3;  // 25 px line pitch with the 28 px font line
constexpr int kTagX = 22;
constexpr int kTagH = 28;
constexpr int kTagRise = 20;  // the tag sits on the top edge, clear of the first line

// Animation timing in ticks of the UI timer.
constexpr uint32_t kTickMs = 50;
constexpr uint32_t kVolumeTicks = 40;
constexpr uint32_t kScrollPauseTicks = 40;
constexpr uint32_t kScrollEndTicks = 60;
constexpr uint32_t kScrollStepTicks = 2;
constexpr uint32_t kBlinkTicks = 3;

struct TalkStep {
    kids_mouth_t mouth;
    uint8_t ticks;
};
// An irregular cycle of about 7 mouth changes per second.
constexpr TalkStep kTalk[] = {
    {KIDS_MOUTH_OPEN_M, 3}, {KIDS_MOUTH_OPEN_S, 2}, {KIDS_MOUTH_OPEN_L, 4}, {KIDS_MOUTH_SMILE, 3},
    {KIDS_MOUTH_OPEN_M, 2}, {KIDS_MOUTH_OPEN_S, 3}, {KIDS_MOUTH_OPEN_L, 4}, {KIDS_MOUTH_SMILE, 2},
    {KIDS_MOUTH_OPEN_S, 3}, {KIDS_MOUTH_OPEN_M, 3}, {KIDS_MOUTH_SMILE, 2},  {KIDS_MOUTH_OPEN_L, 3},
};
constexpr size_t kTalkSteps = sizeof(kTalk) / sizeof(kTalk[0]);

// Effect frames; -1 shows the plain stage.
constexpr int8_t kListenFrames[] = {0, 1, 2, -1};
constexpr int8_t kThinkFrames[] = {0, 1, 2, 2};
constexpr int8_t kZzzFrames[] = {0, 1, 2, -1};
constexpr int8_t kNoteFrames[] = {0, 1, 2};

lv_color_t Hex(uint32_t color) { return lv_color_hex(color); }

bool OneOf(const std::string& value, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (value == name) {
            return true;
        }
    }
    return false;
}

bool IsSpeaking(DeviceState state) {
    return state == kDeviceStateSpeaking || state == kDeviceStateNotifying;
}

bool StartsWithClosingPunct(const char* text) {
    static const char* const kClosing[] = {"，", "。", "！", "？", "、", "：", "；",
                                           "）", "」", "』", "》", "”",  "’",  ",",
                                           ".",  "!",  "?",  ":",  ";",  ")"};
    for (const char* mark : kClosing) {
        if (strncmp(text, mark, strlen(mark)) == 0) {
            return true;
        }
    }
    return false;
}

// Wraps text for the bubble the way the label would, but keeps closing
// punctuation off the start of a line (LVGL breaks CJK text after any
// character): such a line takes the last character of the line before.
std::string WrapBubbleText(const std::string& text, const lv_font_t* font) {
    lv_text_attributes_t attributes;
    lv_text_attributes_init(&attributes);
    attributes.line_space = kLineSpace;
    attributes.max_width = kTextW;
    const char* s = text.c_str();
    const uint32_t length = text.size();
    std::string out;
    uint32_t pos = 0;
    while (pos < length) {
        uint32_t end =
            pos + lv_text_get_next_line(s + pos, length - pos, font, nullptr, &attributes);
        if (end == pos) {
            end = length;
        }
        if (end < length && s[end - 1] != '\n' && StartsWithClosingPunct(s + end)) {
            uint32_t last = end - 1;
            while (last > pos && (s[last] & 0xC0) == 0x80) {
                last--;
            }
            if (last > pos) {
                end = last;
            }
        }
        out.append(s + pos, end - pos);
        if (end < length && s[end - 1] != '\n') {
            out += '\n';
        }
        pos = end;
    }
    return out;
}

// System messages (instructions and errors) read best one clause per line,
// with a link or a version number starting a line of its own.
std::string SplitClauses(const char* text) {
    std::string out;
    for (const char* p = text; *p != '\0'; p++) {
        if (strncmp(p, " http", 5) == 0) {
            out += '\n';
            continue;
        }
        out += *p;
        const char next = p[1];
        if (next == '\0' || next == '\n') {
            continue;
        }
        const bool clause = out.ends_with("，") || out.ends_with("：");
        const bool version = *p == '/' && isdigit(static_cast<unsigned char>(next)) && p > text &&
                             isalnum(static_cast<unsigned char>(p[-1]));
        if (clause || version) {
            out += '\n';
        }
    }
    return out;
}

bool CanBlink(kids_eyes_t eyes) {
    return eyes == KIDS_EYES_OPEN || eyes == KIDS_EYES_WIDE || eyes == KIDS_EYES_LOOK_UP ||
           eyes == KIDS_EYES_SAD;
}

lv_obj_t* CreateBox(lv_obj_t* parent) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    return box;
}

lv_obj_t* CreatePatch(lv_obj_t* stage, int x, int y) {
    lv_obj_t* patch = lv_image_create(stage);
    lv_obj_set_pos(patch, x, y);
    lv_obj_add_flag(patch, LV_OBJ_FLAG_HIDDEN);
    return patch;
}

void SetPatch(lv_obj_t* patch, const lv_image_dsc_t* image) {
    if (image == nullptr) {
        lv_obj_add_flag(patch, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (lv_image_get_src(patch) != image) {
        lv_image_set_src(patch, image);
    }
    lv_obj_remove_flag(patch, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* CreateIcon(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
    lv_obj_t* icon = lv_label_create(parent);
    lv_obj_set_style_text_font(icon, font, 0);
    lv_obj_set_style_text_color(icon, Hex(color), 0);
    lv_label_set_text(icon, "");
    return icon;
}

lv_obj_t* CreatePillLabel(lv_obj_t* pill) {
    lv_obj_t* label = lv_label_create(pill);
    lv_obj_set_style_max_width(label, kPillTextMax, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_text(label, "");
    return label;
}

}  // namespace

KidsDisplay::KidsDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, int offset_x, int offset_y, bool mirror_x,
                         bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {
    // The kids UI draws its own colours; it only takes the fonts from a theme.
    if (current_theme_ == nullptr) {
        current_theme_ = LvglThemeManager::GetInstance().GetTheme("dark");
    }
    if (display_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    // Paint the panel dark before the board turns the backlight on.
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, Hex(KIDS_ART_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_invalidate(screen);
    lv_refr_now(display_);
    // Registered after the corner mask of the base class, so captured tiles
    // are already masked and not yet byte-swapped.
    lv_display_add_event_cb(display_, CaptureFlushEvent, LV_EVENT_FLUSH_START, this);
}

void KidsDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    Display::SetupUI();
    DisplayLockGuard lock(this);
    auto* theme = static_cast<LvglTheme*>(current_theme_);
    const lv_font_t* text_font = theme->text_font()->font();
    const lv_font_t* icon_font = theme->icon_font()->font();

    lv_obj_t* screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, Hex(kText), 0);

    // Stage: sky and mascot, with patches for the expressions and effects.
    emoji_box_ = CreateBox(screen);
    lv_obj_set_size(emoji_box_, KIDS_ART_STAGE_W, KIDS_ART_STAGE_H);
    lv_obj_t* stage = lv_image_create(emoji_box_);
    lv_image_set_src(stage, &kids_art_stage);
    eyes_ = CreatePatch(emoji_box_, KIDS_ART_EYES_X, KIDS_ART_EYES_Y);
    mouth_ = CreatePatch(emoji_box_, KIDS_ART_MOUTH_X, KIDS_ART_MOUTH_Y);
    fx_left_ = CreatePatch(emoji_box_, KIDS_ART_FX_LEFT_X, KIDS_ART_FX_LEFT_Y);
    fx_right_ = CreatePatch(emoji_box_, KIDS_ART_FX_RIGHT_X, KIDS_ART_FX_RIGHT_Y);

    preview_image_ = lv_image_create(screen);
    lv_obj_set_size(preview_image_, width_ / 2, height_ / 2);
    lv_obj_align(preview_image_, LV_ALIGN_TOP_MID, 0, kPillY + kPillH);
    lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);

    // Status row: network icon, status pill, mute and battery icons.
    top_bar_ = CreateBox(screen);
    lv_obj_set_size(top_bar_, LV_HOR_RES, kPillY + kPillH);
    network_label_ = CreateIcon(top_bar_, icon_font, kTextSoft);
    lv_obj_set_pos(network_label_, kIconInset, kIconY);

    lv_obj_t* right_icons = CreateBox(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(right_icons, 2, 0);
    lv_obj_align(right_icons, LV_ALIGN_TOP_RIGHT, -kIconInset, kIconY);
    mute_label_ = CreateIcon(right_icons, icon_font, kTextSoft);
    battery_label_ = CreateIcon(right_icons, icon_font, kTextSoft);

    status_bar_ = CreateBox(top_bar_);
    lv_obj_set_size(status_bar_, LV_SIZE_CONTENT, kPillH);
    lv_obj_set_style_radius(status_bar_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(status_bar_, kPillPadX, 0);
    lv_obj_set_style_pad_top(status_bar_, kPillPadTop, 0);
    lv_obj_set_style_text_color(status_bar_, Hex(kDarkText), 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, kPillY);
    status_label_ = CreatePillLabel(status_bar_);
    notification_label_ = CreatePillLabel(status_bar_);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    volume_pill_ = CreateBox(top_bar_);
    lv_obj_set_size(volume_pill_, LV_SIZE_CONTENT, kPillH);
    lv_obj_set_style_radius(volume_pill_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(volume_pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(volume_pill_, Hex(kBubbleFill), 0);
    lv_obj_set_style_border_width(volume_pill_, kBorder, 0);
    lv_obj_set_style_border_color(volume_pill_, Hex(kPink), 0);
    lv_obj_set_style_pad_hor(volume_pill_, 10, 0);
    lv_obj_set_style_pad_column(volume_pill_, 6, 0);
    lv_obj_set_flex_flow(volume_pill_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(volume_pill_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(volume_pill_, LV_ALIGN_TOP_MID, 0, kPillY);
    lv_obj_add_flag(volume_pill_, LV_OBJ_FLAG_HIDDEN);
    volume_icon_ = CreateIcon(volume_pill_, icon_font, kPink);
    volume_bar_ = CreateBox(volume_pill_);
    lv_obj_set_size(volume_bar_, 78, 15);
    lv_obj_add_event_cb(volume_bar_, VolumeBarDrawEvent, LV_EVENT_DRAW_MAIN, this);

    // Speech bubble. Its label sits in a clipping viewport so long text can
    // scroll without touching the border.
    bottom_bar_ = CreateBox(screen);
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bottom_bar_, Hex(kBubbleFill), 0);
    lv_obj_set_style_border_width(bottom_bar_, kBorder, 0);
    lv_obj_set_style_radius(bottom_bar_, kBubbleRadius, 0);
    lv_obj_set_style_pad_hor(bottom_bar_, kPadX, 0);
    lv_obj_set_style_pad_top(bottom_bar_, kPadTop, 0);
    lv_obj_set_style_pad_bottom(bottom_bar_, kPadBottom, 0);
    viewport_ = CreateBox(bottom_bar_);
    lv_obj_set_width(viewport_, kTextW);
    chat_message_label_ = lv_label_create(viewport_);
    lv_obj_set_width(chat_message_label_, kTextW);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(chat_message_label_, kLineSpace, 0);
    lv_label_set_text(chat_message_label_, "");

    hint_row_ = CreateBox(bottom_bar_);
    lv_obj_set_size(hint_row_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(hint_row_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hint_row_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hint_row_, 8, 0);
    lv_obj_align(hint_row_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* key = lv_label_create(hint_row_);
    lv_label_set_text(key, KIDS_TEXT_KEY);
    lv_obj_set_style_bg_opa(key, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(key, Hex(kLilac), 0);
    lv_obj_set_style_radius(key, 8, 0);
    lv_obj_set_style_pad_hor(key, 7, 0);
    lv_obj_set_style_text_color(key, Hex(kDarkText), 0);
    lv_obj_t* start = lv_label_create(hint_row_);
    lv_label_set_text(start, KIDS_TEXT_START);
    lv_obj_set_style_text_color(start, Hex(kTextSoft), 0);

    tag_ = lv_label_create(screen);
    lv_label_set_text(tag_, KIDS_TEXT_USER_TAG);
    lv_obj_set_style_bg_opa(tag_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tag_, Hex(kPink), 0);
    lv_obj_set_style_radius(tag_, kTagH / 2, 0);
    lv_obj_set_style_pad_hor(tag_, 9, 0);
    lv_obj_set_style_text_color(tag_, Hex(kDarkText), 0);
    lv_obj_add_flag(tag_, LV_OBJ_FLAG_HIDDEN);

    low_battery_popup_ = CreateBox(screen);
    lv_obj_set_size(low_battery_popup_, 200, 52);
    lv_obj_set_pos(low_battery_popup_, 20, 228);
    lv_obj_set_style_bg_opa(low_battery_popup_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, Hex(kPopupFill), 0);
    lv_obj_set_style_border_width(low_battery_popup_, kBorder, 0);
    lv_obj_set_style_border_color(low_battery_popup_, Hex(kCoral), 0);
    lv_obj_set_style_radius(low_battery_popup_, 16, 0);
    lv_obj_set_style_pad_column(low_battery_popup_, 8, 0);
    lv_obj_set_flex_flow(low_battery_popup_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(low_battery_popup_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t* battery_icon = CreateIcon(low_battery_popup_, icon_font, kCoral);
    lv_label_set_text(battery_icon, MATERIAL_SYMBOLS_BATTERY_ANDROID_0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    status_ = Lang::Strings::INITIALIZING;
    ui_ready_ = true;
    Refresh();
    lv_timer_create(TimerCallback, kTickMs, this);
}

DeviceState KidsDisplay::State() const {
    if (injected_state_ >= 0) {
        return static_cast<DeviceState>(injected_state_);
    }
    return Application::GetInstance().GetDeviceState();
}

void KidsDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    status_ = status != nullptr ? status : "";
    // The base class also shows the status label and hides a notification.
    LvglDisplay::SetStatus(status_.c_str());
    Refresh();
}

void KidsDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    emotion_ = emotion != nullptr ? emotion : "";
    Refresh();
}

bool KidsDisplay::AddTextGlyphs(const std::vector<TextGlyph>& glyphs, uint8_t bpp) {
    // Application pushes a glyph batch right before the message it belongs
    // to, and on this board every batch replaces the active one. Keep it until
    // that message arrives: only the child's words may replace the glyphs.
    DisplayLockGuard lock(this);
    staged_glyphs_ = glyphs;
    staged_bpp_ = bpp;
    glyphs_staged_ = true;
    return true;
}

void KidsDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (glyphs_staged_) {
        glyphs_staged_ = false;
        std::vector<TextGlyph> glyphs;
        glyphs.swap(staged_glyphs_);
        if (strcmp(role, "user") == 0) {
            LvglDisplay::AddTextGlyphs(glyphs, staged_bpp_);
        }
    }
    const char* text = content != nullptr ? content : "";
    if (text[0] == '\0' && activation_shown_) {
        ClearActivationCode();
    }
    if (strcmp(role, "user") == 0) {
        user_text_ = text;
    } else if (strcmp(role, "system") == 0) {
        system_text_ = text;
    }
    // The assistant's words (and notification subtitles) are never shown.
    Refresh();
}

void KidsDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    if (activation_shown_) {
        ClearActivationCode();
    }
    user_text_.clear();
    system_text_.clear();
    Refresh();
}

void KidsDisplay::ShowActivationCode(const char* code, const char* message) {
    DisplayLockGuard lock(this);
    LcdDisplay::ShowActivationCode(code, message);
    activation_shown_ = true;
    for (lv_obj_t* label :
         {activation_hint_label_, activation_site_label_, activation_code_label_}) {
        if (label != nullptr) {
            lv_obj_set_style_text_color(label, Hex(kText), 0);
        }
    }
    Refresh();
}

void KidsDisplay::ClearActivationCode() {
    DisplayLockGuard lock(this);
    LcdDisplay::ClearActivationCode();
    activation_shown_ = false;
    bubble_mode_ = BubbleMode::kNone;
    Refresh();
}

void KidsDisplay::SetTheme(Theme* theme) {
    if (theme == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    // Only the text font follows the theme (it changes when the asset font
    // is loaded); the colours stay dark whatever theme is stored.
    auto text_font = static_cast<LvglTheme*>(theme)->text_font();
    if (ui_ready_ && text_font != nullptr && text_font->font() != nullptr) {
        lv_obj_set_style_text_font(lv_screen_active(), text_font->font(), 0);
        bubble_mode_ = BubbleMode::kNone;
        Refresh();
    }
    Display::SetTheme(theme);
}

void KidsDisplay::SetHideSubtitle(bool hide) {
    // The bubble never shows subtitles: it shows the child's words and hints.
    (void)hide;
}

void KidsDisplay::UpdateStatusBar(bool update_all) {
    LvglDisplay::UpdateStatusBar(update_all);
    {
        DisplayLockGuard lock(this);
        if (ui_ready_ && muted_ != pill_narrow_) {
            SetPillTextWidth(muted_);
        }
    }
    if (!update_all) {
        PollConsole();
    }
}

void KidsDisplay::SetPillTextWidth(bool narrow) {
    pill_narrow_ = narrow;
    int width = narrow ? kPillTextMaxMuted : kPillTextMax;
    lv_obj_set_style_max_width(status_label_, width, 0);
    lv_obj_set_style_max_width(notification_label_, width, 0);
}

void KidsDisplay::ShowVolume(int volume) {
    DisplayLockGuard lock(this);
    if (!ui_ready_) {
        return;
    }
    volume_ = volume;
    lv_label_set_text(volume_icon_,
                      volume > 0 ? MATERIAL_SYMBOLS_VOLUME_UP : MATERIAL_SYMBOLS_VOLUME_OFF);
    lv_obj_invalidate(volume_bar_);
    lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(volume_pill_, LV_OBJ_FLAG_HIDDEN);
    volume_until_ = tick_ + kVolumeTicks;
}

void KidsDisplay::VolumeBarDrawEvent(lv_event_t* event) {
    auto* self = static_cast<KidsDisplay*>(lv_event_get_user_data(event));
    lv_obj_t* bar = lv_event_get_target_obj(event);
    lv_layer_t* layer = lv_event_get_layer(event);
    lv_area_t box;
    lv_obj_get_coords(bar, &box);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = 2;
    const int lit = (self->volume_ + 9) / 10;
    for (int i = 0; i < 10; i++) {
        // A rising staircase of ten steps.
        lv_area_t step = {box.x1 + i * 8, box.y2 - 5 - i, box.x1 + i * 8 + 5, box.y2};
        dsc.bg_color = Hex(i < lit ? kPink : kDim);
        lv_draw_rect(layer, &dsc, &step);
    }
}

void KidsDisplay::TrackState() {
    DeviceState state = State();
    if (state == last_state_) {
        return;
    }
    last_state_ = state;
    // Her words stay through the answer and go away when a new turn starts.
    if (state == kDeviceStateListening || state == kDeviceStateConnecting ||
        state == kDeviceStateIdle) {
        user_text_.clear();
    }
    if (state == kDeviceStateIdle) {
        system_text_.clear();
    }
}

void KidsDisplay::Refresh() {
    TrackState();
    if (!ui_ready_) {
        return;
    }
    DeviceState state = State();
    ApplyPill(state);
    ApplyBubble(state);
    Look look = ComputeLook(state);
    if (look.talk && !look_.talk) {
        talk_step_ = 0;
        talk_next_ = tick_ + kTalk[0].ticks;
    }
    look_ = look;
    ApplyLook();
}

void KidsDisplay::ApplyPill(DeviceState state) {
    const char* text = status_.c_str();
    uint32_t color = kYellow;
    if (status_ == Lang::Strings::ERROR) {
        text = KIDS_TEXT_ERROR;
        color = kCoral;
    } else if (state == kDeviceStateIdle) {
        color = kLilac;
    } else if (state == kDeviceStateListening) {
        text = user_text_.empty() ? KIDS_TEXT_LISTENING : KIDS_TEXT_WAIT;
        color = user_text_.empty() ? kMint : kYellow;
    } else if (IsSpeaking(state)) {
        text = KIDS_TEXT_SPEAKING;
        color = kPink;
    }
    if (strcmp(lv_label_get_text(status_label_), text) != 0) {
        lv_label_set_text(status_label_, text);
    }
    if (color != pill_color_) {
        pill_color_ = color;
        lv_obj_set_style_bg_color(status_bar_, Hex(color), 0);
    }
}

void KidsDisplay::ApplyBubble(DeviceState state) {
    if (activation_shown_) {
        lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tag_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    BubbleMode mode = BubbleMode::kHint;
    const char* text = KIDS_TEXT_WAIT;
    if (!system_text_.empty()) {
        mode = BubbleMode::kSystem;
        text = system_text_.c_str();
    } else if (!user_text_.empty()) {
        mode = BubbleMode::kUser;
        text = user_text_.c_str();
    } else if (state == kDeviceStateListening) {
        mode = BubbleMode::kListen;
        text = KIDS_TEXT_LISTEN_HINT;
    } else if (state == kDeviceStateIdle) {
        mode = BubbleMode::kHintKey;
        text = "";
    } else if (IsSpeaking(state)) {
        text = KIDS_TEXT_SPEAK_HINT;
    }
    if (mode == bubble_mode_ && bubble_text_ == text) {
        return;
    }
    bubble_mode_ = mode;
    bubble_text_ = text;

    uint32_t border = kLilac;
    uint32_t color = kTextSoft;
    switch (mode) {
        case BubbleMode::kListen:
            border = kMint;
            color = kText;
            break;
        case BubbleMode::kUser:
            border = kPink;
            color = kText;
            break;
        case BubbleMode::kSystem:
            border = kYellow;
            color = kText;
            break;
        default:
            break;
    }
    lv_obj_set_style_border_color(bottom_bar_, Hex(border), 0);
    lv_obj_set_style_text_color(chat_message_label_, Hex(color), 0);
    const lv_font_t* font = lv_obj_get_style_text_font(chat_message_label_, LV_PART_MAIN);
    std::string shown = mode == BubbleMode::kSystem ? SplitClauses(text) : std::string(text);
    lv_label_set_text(chat_message_label_, WrapBubbleText(shown, font).c_str());
    if (mode == BubbleMode::kHintKey) {
        lv_obj_add_flag(viewport_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(hint_row_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(viewport_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(hint_row_, LV_OBJ_FLAG_HIDDEN);
    }
    LayoutBubble();
    lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
    if (mode == BubbleMode::kUser) {
        lv_obj_remove_flag(tag_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(tag_, LV_OBJ_FLAG_HIDDEN);
    }
}

void KidsDisplay::LayoutBubble() {
    // Three lines by default, four when needed; longer text scrolls.
    const lv_font_t* font = lv_obj_get_style_text_font(chat_message_label_, LV_PART_MAIN);
    const int line_h = lv_font_get_line_height(font);
    const int three = 3 * line_h + 2 * kLineSpace;
    const int four = 4 * line_h + 3 * kLineSpace;
    int text_h = line_h;
    if (bubble_mode_ != BubbleMode::kHintKey) {
        lv_point_t size;
        lv_text_get_size(&size, lv_label_get_text(chat_message_label_), font, 0, kLineSpace, kTextW,
                         LV_TEXT_FLAG_NONE);
        text_h = size.y;
    }
    const int view_h = text_h <= three ? three : four;
    const int box_h = view_h + 2 * kBorder + kPadTop + kPadBottom;
    const int box_y = kBubbleBottom - box_h;
    lv_obj_set_pos(bottom_bar_, kBubbleX, box_y);
    lv_obj_set_size(bottom_bar_, kBubbleW, box_h);
    lv_obj_set_height(viewport_, view_h);
    lv_obj_set_y(chat_message_label_, text_h < view_h ? (view_h - text_h) / 2 : 0);
    lv_obj_set_pos(tag_, kTagX, box_y - kTagRise);
    scroll_max_ = text_h > view_h ? text_h - view_h : 0;
    scroll_y_ = 0;
    scroll_phase_ = 0;
    scroll_until_ = tick_ + kScrollPauseTicks;
}

KidsDisplay::Look KidsDisplay::ComputeLook(DeviceState state) const {
    if (state == kDeviceStateListening) {
        if (user_text_.empty()) {
            return {KIDS_EYES_WIDE, KIDS_MOUTH_SMILE, KIDS_FX_LISTEN, false};
        }
        return {KIDS_EYES_LOOK_UP, KIDS_MOUTH_O, KIDS_FX_THINK, false};
    }
    Look look = {KIDS_EYES_OPEN, KIDS_MOUTH_CAT, KIDS_FX_NONE, false};
    const std::string& e = emotion_;
    if (OneOf(e, {"sad", "crying", "angry", "warning", "cancel", "cloud_off"})) {
        look = {KIDS_EYES_SAD, KIDS_MOUTH_SAD, KIDS_FX_NONE, false};
    } else if (OneOf(e, {"happy", "funny"})) {
        look = {KIDS_EYES_HAPPY, KIDS_MOUTH_SMILE, KIDS_FX_NONE, false};
    } else if (e == "laughing") {
        look = {KIDS_EYES_HAPPY, KIDS_MOUTH_OPEN_M, KIDS_FX_NONE, false};
    } else if (e == "loving") {
        look = {KIDS_EYES_HEART, KIDS_MOUTH_SMILE, KIDS_FX_NONE, false};
    } else if (e == "kissy") {
        look = {KIDS_EYES_HEART, KIDS_MOUTH_O, KIDS_FX_NONE, false};
    } else if (e == "winking") {
        look = {KIDS_EYES_WINK, KIDS_MOUTH_SMILE, KIDS_FX_NONE, false};
    } else if (OneOf(e, {"surprised", "shocked"})) {
        look = {KIDS_EYES_WIDE, KIDS_MOUTH_OPEN_S, KIDS_FX_NONE, false};
    } else if (OneOf(e, {"thinking", "confused"})) {
        look = {KIDS_EYES_LOOK_UP, KIDS_MOUTH_O, KIDS_FX_NONE, false};
    } else if (e == "sleepy") {
        look = {KIDS_EYES_CLOSED, KIDS_MOUTH_CAT, KIDS_FX_ZZZ, false};
    }
    if (IsSpeaking(state)) {
        look.mouth = KIDS_MOUTH_SMILE;
        look.fx = KIDS_FX_NOTES;
        look.talk = true;
    }
    return look;
}

void KidsDisplay::ApplyLook() {
    kids_eyes_t eyes = look_.eyes;
    if (!posed_ && tick_ < blink_end_ && CanBlink(eyes)) {
        eyes = KIDS_EYES_CLOSED;
    }
    kids_mouth_t mouth = look_.mouth;
    if (look_.talk) {
        mouth = posed_ ? KIDS_MOUTH_OPEN_M : kTalk[talk_step_].mouth;
    }
    int frame = -1;
    switch (look_.fx) {
        case KIDS_FX_LISTEN:
            frame = kListenFrames[(tick_ / 8) % 4];
            break;
        case KIDS_FX_THINK:
            frame = kThinkFrames[(tick_ / 10) % 4];
            break;
        case KIDS_FX_ZZZ:
            frame = kZzzFrames[(tick_ / 14) % 4];
            break;
        case KIDS_FX_NOTES:
            frame = kNoteFrames[(tick_ / 8) % 3];
            break;
        default:
            break;
    }
    if (posed_ && look_.fx != KIDS_FX_NONE) {
        frame = KIDS_FX_FRAMES - 1;
    }
    SetPatch(eyes_, kids_art_eyes[eyes]);
    SetPatch(mouth_, kids_art_mouth[mouth]);
    SetPatch(fx_left_, frame < 0 ? nullptr : kids_art_fx_left[look_.fx][frame]);
    SetPatch(fx_right_, frame < 0 ? nullptr : kids_art_fx_right[look_.fx][frame]);
}

void KidsDisplay::TimerCallback(lv_timer_t* timer) {
    static_cast<KidsDisplay*>(lv_timer_get_user_data(timer))->OnTick();
}

void KidsDisplay::OnTick() {
    // Runs in the LVGL task, which holds the display lock.
    if (!ui_ready_ || capture_active_) {
        return;
    }
    tick_++;
    if (State() != last_state_) {
        Refresh();
    }
    if (volume_until_ != 0 && tick_ >= volume_until_) {
        volume_until_ = 0;
        lv_obj_add_flag(volume_pill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
    }

    if (scroll_max_ > 0 && tick_ >= scroll_until_) {
        if (scroll_phase_ == 0) {
            scroll_phase_ = 1;
        } else if (scroll_phase_ == 1) {
            scroll_y_++;
            scroll_until_ = tick_ + kScrollStepTicks;
            if (scroll_y_ >= scroll_max_) {
                scroll_phase_ = 2;
                scroll_until_ = tick_ + kScrollEndTicks;
            }
        } else {
            scroll_y_ = 0;
            scroll_phase_ = 0;
            scroll_until_ = tick_ + kScrollPauseTicks;
        }
        lv_obj_set_y(chat_message_label_, -scroll_y_);
    }

    if (posed_) {
        return;
    }
    if (tick_ >= next_blink_) {
        blink_end_ = tick_ + kBlinkTicks;
        next_blink_ = tick_ + 60 + esp_random() % 40;
    }
    if (look_.talk && tick_ >= talk_next_) {
        talk_step_ = (talk_step_ + 1) % kTalkSteps;
        talk_next_ = tick_ + kTalk[talk_step_].ticks;
    }
    ApplyLook();
}

#endif  // CONFIG_AI_PASSPORT_KIDS_UI
