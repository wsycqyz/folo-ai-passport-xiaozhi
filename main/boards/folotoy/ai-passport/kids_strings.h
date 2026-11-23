#ifndef KIDS_STRINGS_H
#define KIDS_STRINGS_H

// Fixed texts of the kids UI. They are drawn before the asset font is loaded,
// so KIDS_TEXT_* may only use ASCII and characters of the built-in "basic"
// font. scripts/tests/test_kids_ui.py checks this.
#define KIDS_TEXT_LISTENING "我在听"
#define KIDS_TEXT_WAIT "请稍等"
#define KIDS_TEXT_SPEAKING "我在说"
#define KIDS_TEXT_ERROR "出错了"
#define KIDS_TEXT_KEY "OK"
#define KIDS_TEXT_START "开始说话"
#define KIDS_TEXT_LISTEN_HINT "我在听，请说话"
#define KIDS_TEXT_SPEAK_HINT "我在说话"
#define KIDS_TEXT_USER_TAG "我说"

// Sample words of the KUI DEMO gallery. The gallery runs once the asset font
// is loaded, so KIDS_DEMO_* use the "common" charset (also checked).
#define KIDS_DEMO_SHORT "给我讲个故事吧"
#define KIDS_DEMO_THREE_LINES "为什么天上的星星到了晚上就会一闪一闪地发光呀？"
#define KIDS_DEMO_LONG                                   \
    "我今天在学校画了一只会飞的小猫，它有彩虹色的翅膀，" \
    "晚上会飞到月亮上去跳舞，你能帮它起一个好听的名字吗？"
#define KIDS_DEMO_MIXED "我叫Lily，今年8岁！What is 12+30?"
#define KIDS_DEMO_ASSISTANT "好呀，我来讲一个小兔子的故事。"
#define KIDS_DEMO_ERROR_DETAIL "HTTP 503"
#define KIDS_DEMO_CODE "123456"

#endif  // KIDS_STRINGS_H
