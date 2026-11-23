#ifndef KIDS_DISPLAY_H
#define KIDS_DISPLAY_H

#include "device_state.h"
#include "display/lcd_display.h"
#include "kids_art.h"

#include <atomic>
#include <string>
#include <vector>

// Kids UI of the AI Passport ("starry night"): a code-drawn mascot on a dark
// sky, a status pill, and a speech bubble that shows only the child's words.
class KidsDisplay : public SpiLcdDisplay {
public:
    KidsDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);

    void SetupUI() override;
    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;
    void SetChatMessage(const char* role, const char* content) override;
    void ClearChatMessages() override;
    void ShowActivationCode(const char* code, const char* message) override;
    void ClearActivationCode() override;
    void SetTheme(Theme* theme) override;
    void SetHideSubtitle(bool hide) override;
    void UpdateStatusBar(bool update_all = false) override;
    bool AddTextGlyphs(const std::vector<TextGlyph>& glyphs, uint8_t bpp) override;

    // Shows a speaker icon and a 10-step bar in the status pill for 2 seconds.
    void ShowVolume(int volume);

private:
    enum class BubbleMode { kNone, kHintKey, kHint, kListen, kUser, kSystem };

    struct Look {
        kids_eyes_t eyes;
        kids_mouth_t mouth;
        kids_fx_t fx;
        bool talk;
    };

    // UI model, guarded by the display lock.
    std::string status_;
    std::string emotion_;
    std::string user_text_;
    std::string system_text_;
    DeviceState last_state_ = kDeviceStateUnknown;
    int injected_state_ = -1;  // >= 0: state scripted by the KUI DEMO gallery
    bool activation_shown_ = false;
    bool ui_ready_ = false;
    std::vector<TextGlyph> staged_glyphs_;
    uint8_t staged_bpp_ = 0;
    bool glyphs_staged_ = false;

    // Widgets that the base class does not own.
    lv_obj_t* eyes_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* fx_left_ = nullptr;
    lv_obj_t* fx_right_ = nullptr;
    lv_obj_t* viewport_ = nullptr;
    lv_obj_t* hint_row_ = nullptr;
    lv_obj_t* tag_ = nullptr;
    lv_obj_t* volume_pill_ = nullptr;
    lv_obj_t* volume_icon_ = nullptr;
    lv_obj_t* volume_bar_ = nullptr;

    // Applied state of the widgets.
    BubbleMode bubble_mode_ = BubbleMode::kNone;
    std::string bubble_text_;
    uint32_t pill_color_ = 0;
    bool pill_narrow_ = false;
    Look look_ = {KIDS_EYES_OPEN, KIDS_MOUTH_CAT, KIDS_FX_NONE, false};
    int volume_ = 0;
    uint32_t volume_until_ = 0;

    // Animation, advanced by the UI timer in the LVGL task.
    uint32_t tick_ = 0;
    uint32_t next_blink_ = 60;
    uint32_t blink_end_ = 0;
    size_t talk_step_ = 0;
    uint32_t talk_next_ = 0;
    bool posed_ = false;  // the gallery holds a fixed animation frame
    int scroll_y_ = 0;
    int scroll_max_ = 0;
    int scroll_phase_ = 0;
    uint32_t scroll_until_ = 0;

    // Maintenance console and screen capture.
    char line_[64] = {};
    size_t line_len_ = 0;
    char capture_name_[24] = {};
    int capture_tiles_ = 0;
    std::atomic<bool> capture_active_{false};
    std::atomic<bool> capture_done_{false};

    DeviceState State() const;
    void Refresh();
    void TrackState();
    void ApplyPill(DeviceState state);
    void ApplyBubble(DeviceState state);
    void LayoutBubble();
    Look ComputeLook(DeviceState state) const;
    void ApplyLook();
    void OnTick();
    void SetPillTextWidth(bool narrow);

    static void TimerCallback(lv_timer_t* timer);
    static void VolumeBarDrawEvent(lv_event_t* event);
    static void CaptureFlushEvent(lv_event_t* event);

    void PollConsole();
    void HandleCommand(const char* line);
    void RequestCapture(const char* name);
    void WaitCapture();
    void StreamTile(lv_display_t* display, const lv_area_t* area);
    void RunDemo();
    void DemoCapture(const char* name);
};

#endif  // KIDS_DISPLAY_H
