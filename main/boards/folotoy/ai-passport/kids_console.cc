#include "sdkconfig.h"

#if CONFIG_AI_PASSPORT_KIDS_UI

// Maintenance console of the kids UI on the USB Serial/JTAG port:
//   KUI VOL n   set and store the output volume (0..100)
//   KUI SHOT    stream the current screen
//   KUI DEMO    stream a scripted gallery of the screens, then restore
//   KUI CHAT    toggle the chat state like the OK key
// A frame is streamed from the LVGL flush path as text lines:
//   KUI:FRAME <name> <width> <height>
//   KUI:TILE <x1> <y1> <x2> <y2> <crc32> <base64 of little-endian RGB565 rows>
//   KUI:END <name> <tiles>

#include "kids_display.h"

#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <string>

#include "application.h"
#include "assets/lang_config.h"
#include "audio_codec.h"
#include "board.h"
#include "kids_strings.h"
#include "system_info.h"

#define TAG "KidsConsole"

namespace {

uint32_t Crc32(uint32_t crc, const uint8_t* data, size_t length) {
    static const uint32_t kTable[16] = {
        0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
        0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
        0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
    };
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        crc = (crc >> 4) ^ kTable[crc & 0x0F];
        crc = (crc >> 4) ^ kTable[crc & 0x0F];
    }
    return crc;
}

// Base64 encoder that writes to stdout through a small buffer.
class Base64Writer {
public:
    void Write(const uint8_t* data, size_t length) {
        for (size_t i = 0; i < length; i++) {
            group_[group_len_++] = data[i];
            if (group_len_ == 3) {
                EmitGroup();
            }
        }
    }

    void Finish() {
        if (group_len_ > 0) {
            size_t used = group_len_;
            while (group_len_ < 3) {
                group_[group_len_++] = 0;
            }
            EmitGroup();
            for (size_t i = used + 1; i < 4; i++) {
                out_[out_len_ - 4 + i] = '=';
            }
        }
        fwrite(out_, 1, out_len_, stdout);
        out_len_ = 0;
    }

private:
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t group_[3] = {};
    size_t group_len_ = 0;
    char out_[192] = {};
    size_t out_len_ = 0;

    void EmitGroup() {
        if (out_len_ + 4 > sizeof(out_)) {
            fwrite(out_, 1, out_len_, stdout);
            out_len_ = 0;
        }
        uint32_t v = (group_[0] << 16) | (group_[1] << 8) | group_[2];
        out_[out_len_++] = kAlphabet[(v >> 18) & 0x3F];
        out_[out_len_++] = kAlphabet[(v >> 12) & 0x3F];
        out_[out_len_++] = kAlphabet[(v >> 6) & 0x3F];
        out_[out_len_++] = kAlphabet[v & 0x3F];
        group_len_ = 0;
    }
};

std::string ClockText() {
    char text[8] = "12:30";
    time_t now = time(nullptr);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    if (tm_now.tm_year >= 2025 - 1900) {
        strftime(text, sizeof(text), "%H:%M", &tm_now);
    }
    return text;
}

// The Wi-Fi setup hint exactly as WifiBoard builds it (SSID prefix "Xiaozhi").
std::string WifiSetupHint() {
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ssid[16];
    snprintf(ssid, sizeof(ssid), "Xiaozhi-%02X%02X", mac[4], mac[5]);
    return std::string(Lang::Strings::CONNECT_TO_HOTSPOT) + ssid +
           Lang::Strings::ACCESS_VIA_BROWSER + "http://192.168.4.1";
}

}  // namespace

void KidsDisplay::PollConsole() {
    // No USB Serial/JTAG driver is installed, so read() on the console only
    // returns bytes already in the receive FIFO and never blocks.
    const int fd = fileno(stdin);
    char c;
    for (int i = 0; i < 64 && read(fd, &c, 1) == 1; i++) {
        if (c == '\n' || c == '\r') {
            if (line_len_ > 0) {
                line_[line_len_] = '\0';
                line_len_ = 0;
                HandleCommand(line_);
            }
        } else if (line_len_ < sizeof(line_) - 1) {
            line_[line_len_++] = c;
        }
    }
}

void KidsDisplay::HandleCommand(const char* line) {
    int value = -1;
    if (sscanf(line, "KUI VOL %d", &value) == 1 && value >= 0 && value <= 100) {
        Board::GetInstance().GetAudioCodec()->SetOutputVolume(value);
        printf("KUI:VOL %d OK\n", value);
    } else if (strcmp(line, "KUI SHOT") == 0) {
        RequestCapture("shot");
    } else if (strcmp(line, "KUI DEMO") == 0) {
        RunDemo();
    } else if (strcmp(line, "KUI CHAT") == 0) {
        Application::GetInstance().ToggleChatState();
        printf("KUI:CHAT OK\n");
    } else if (strncmp(line, "KUI", 3) == 0) {
        printf("KUI:ERR unknown command\n");
    }
}

void KidsDisplay::RequestCapture(const char* name) {
    DisplayLockGuard lock(this);
    if (capture_active_) {
        return;
    }
    strlcpy(capture_name_, name, sizeof(capture_name_));
    capture_tiles_ = 0;
    capture_done_ = false;
    capture_active_ = true;
    // The next refresh renders the whole screen and streams every tile.
    lv_obj_invalidate(lv_screen_active());
}

void KidsDisplay::WaitCapture() {
    for (int i = 0; i < 500 && !capture_done_; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (!capture_done_) {
        DisplayLockGuard lock(this);
        capture_active_ = false;
        printf("KUI:ERR capture timeout\n");
    }
}

void KidsDisplay::CaptureFlushEvent(lv_event_t* event) {
    auto* self = static_cast<KidsDisplay*>(lv_event_get_user_data(event));
    if (self->capture_active_) {
        self->StreamTile(static_cast<lv_display_t*>(lv_event_get_target(event)),
                         static_cast<const lv_area_t*>(lv_event_get_param(event)));
    }
}

void KidsDisplay::StreamTile(lv_display_t* display, const lv_area_t* area) {
    // Runs in the LVGL task while it renders, so the UI cannot change midway.
    lv_draw_buf_t* buf = lv_display_get_buf_active(display);
    if (area == nullptr || buf == nullptr || buf->data == nullptr) {
        return;
    }
    const uint32_t row_bytes = lv_area_get_width(area) * sizeof(uint16_t);
    const int rows = lv_area_get_height(area);
    const uint32_t stride = buf->header.stride;

    flockfile(stdout);
    if (capture_tiles_ == 0) {
        printf("KUI:FRAME %s %d %d\n", capture_name_,
               static_cast<int>(lv_display_get_horizontal_resolution(display)),
               static_cast<int>(lv_display_get_vertical_resolution(display)));
    }
    uint32_t crc = 0xFFFFFFFF;
    for (int y = 0; y < rows; y++) {
        crc = Crc32(crc, buf->data + y * stride, row_bytes);
    }
    printf("KUI:TILE %d %d %d %d %08lx ", static_cast<int>(area->x1), static_cast<int>(area->y1),
           static_cast<int>(area->x2), static_cast<int>(area->y2),
           static_cast<unsigned long>(crc ^ 0xFFFFFFFF));
    Base64Writer writer;
    for (int y = 0; y < rows; y++) {
        writer.Write(buf->data + y * stride, row_bytes);
    }
    writer.Finish();
    fputc('\n', stdout);
    capture_tiles_++;
    if (lv_display_flush_is_last(display)) {
        printf("KUI:END %s %d\n", capture_name_, capture_tiles_);
        capture_active_ = false;
        capture_done_ = true;
    }
    fflush(stdout);
    funlockfile(stdout);
}

void KidsDisplay::DemoCapture(const char* name) {
    RequestCapture(name);
    WaitCapture();
}

void KidsDisplay::RunDemo() {
    if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
        printf("KUI:ERR demo needs the idle state\n");
        return;
    }
    std::string status, emotion, user_text, system_text;
    {
        DisplayLockGuard lock(this);
        status = status_;
        emotion = emotion_;
        user_text = user_text_;
        system_text = system_text_;
        posed_ = true;
    }
    printf("KUI:DEMO BEGIN\n");

    // Each step drives the display the way Application does, then captures.
    auto inject = [this](DeviceState state) {
        DisplayLockGuard lock(this);
        injected_state_ = state;
    };

    inject(kDeviceStateStarting);
    SetStatus(Lang::Strings::INITIALIZING);
    SetEmotion("neutral");
    SetChatMessage("system", SystemInfo::GetUserAgent().c_str());
    DemoCapture("boot");

    inject(kDeviceStateWifiConfiguring);
    SetStatus(Lang::Strings::WIFI_CONFIG_MODE);
    SetEmotion("gear");
    SetChatMessage("system", WifiSetupHint().c_str());
    DemoCapture("wifi_setup");

    inject(kDeviceStateConnecting);
    SetStatus(Lang::Strings::CONNECTING);
    SetEmotion("neutral");
    SetChatMessage("system", "");
    DemoCapture("connecting");

    inject(kDeviceStateIdle);
    SetStatus(ClockText().c_str());
    ClearChatMessages();
    SetEmotion("neutral");
    DemoCapture("idle");

    inject(kDeviceStateListening);
    SetStatus(Lang::Strings::LISTENING);
    SetEmotion("neutral");
    DemoCapture("listening");

    SetChatMessage("user", KIDS_DEMO_SHORT);
    DemoCapture("listening_words");

    inject(kDeviceStateSpeaking);
    SetStatus(Lang::Strings::SPEAKING);
    DemoCapture("speaking_words");

    SetChatMessage("assistant", KIDS_DEMO_ASSISTANT);
    DemoCapture("assistant_hidden");

    inject(kDeviceStateIdle);
    SetStatus(Lang::Strings::STANDBY);
    ClearChatMessages();
    inject(kDeviceStateSpeaking);
    SetStatus(Lang::Strings::SPEAKING);
    DemoCapture("speaking_no_words");

    SetEmotion("loving");
    DemoCapture("speaking_loving");

    inject(kDeviceStateListening);
    SetStatus(Lang::Strings::LISTENING);
    SetEmotion("neutral");
    SetChatMessage("user", KIDS_DEMO_THREE_LINES);
    DemoCapture("three_lines");

    SetChatMessage("user", KIDS_DEMO_MIXED);
    DemoCapture("mixed_text");

    inject(kDeviceStateSpeaking);
    SetStatus(Lang::Strings::SPEAKING);
    SetChatMessage("user", KIDS_DEMO_LONG);
    DemoCapture("long_start");
    // Let the bubble scroll by itself until it holds at the end.
    for (int i = 0; i < 400; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
        DisplayLockGuard lock(this);
        if (scroll_phase_ == 2) {
            break;
        }
    }
    DemoCapture("long_end");

    inject(kDeviceStateIdle);
    SetStatus(Lang::Strings::ERROR);
    SetEmotion("cloud_off");
    char error[200];
    snprintf(error, sizeof(error), Lang::Strings::CHECK_NEW_VERSION_FAILED, 10,
             KIDS_DEMO_ERROR_DETAIL);
    SetChatMessage("system", error);
    DemoCapture("error");

    SetStatus(ClockText().c_str());
    SetEmotion("neutral");
    SetChatMessage("system", "");
    ShowVolume(60);
    DemoCapture("volume");

    bool popup_hidden;
    {
        DisplayLockGuard lock(this);
        volume_until_ = 0;
        lv_obj_add_flag(volume_pill_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        popup_hidden = lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    }
    DemoCapture("low_battery");
    {
        DisplayLockGuard lock(this);
        if (popup_hidden) {
            lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (const char* name : {"sleepy", "happy", "loving", "sad"}) {
        SetEmotion(name);
        DemoCapture(name);
    }

    SetEmotion("neutral");
    std::string version = std::string(Lang::Strings::VERSION) + esp_app_get_description()->version;
    ShowNotification(version.c_str());
    DemoCapture("notification");

    // Application::ShowActivationCode: an alert, then the base class page.
    inject(kDeviceStateActivating);
    SetStatus(Lang::Strings::ACTIVATION);
    SetEmotion("link");
    SetChatMessage("system", KIDS_DEMO_CODE);
    ShowActivationCode(KIDS_DEMO_CODE, KIDS_DEMO_CODE);
    DemoCapture("activation");
    ClearActivationCode();

    {
        DisplayLockGuard lock(this);
        injected_state_ = -1;
        last_state_ = State();
        posed_ = false;
        status_ = status;
        emotion_ = emotion;
        user_text_ = user_text;
        system_text_ = system_text;
        LvglDisplay::SetStatus(status_.c_str());
        Refresh();
    }
    printf("KUI:DEMO END\n");
}

#endif  // CONFIG_AI_PASSPORT_KIDS_UI
