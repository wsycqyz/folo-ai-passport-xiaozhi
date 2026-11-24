import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LOCALES = ROOT / "main" / "assets" / "locales"
BASIC_CHARSET = ROOT / "managed_components" / "78__xiaozhi-fonts" / "charsets" / "basic.json"


class LocaleTest(unittest.TestCase):
    def test_chinese_strings_render_with_the_builtin_font(self):
        # The built-in text font is the only font until the assets partition is
        # loaded, or at all if it is missing, and it has just the "basic"
        # charset: a character outside it is drawn as nothing.
        if not BASIC_CHARSET.exists():
            self.skipTest("managed_components/78__xiaozhi-fonts is not downloaded")
        basic = set(json.loads(BASIC_CHARSET.read_text(encoding="utf-8"))["codepoints"])
        for lang in ("zh-CN", "zh-TW"):
            path = LOCALES / lang / "language.json"
            strings = json.loads(path.read_text(encoding="utf-8"))["strings"]
            for key, text in strings.items():
                with self.subTest(lang=lang, key=key):
                    missing = [c for c in text if ord(c) > 0x7F and ord(c) not in basic]
                    self.assertEqual(missing, [], f"{key} uses characters missing from the basic font")


if __name__ == "__main__":
    unittest.main()
