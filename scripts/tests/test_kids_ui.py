import importlib.util
import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BOARD = ROOT / "main" / "boards" / "folotoy" / "ai-passport"
CHARSETS = ROOT / "managed_components" / "78__xiaozhi-fonts" / "charsets"


def load_generator():
    spec = importlib.util.spec_from_file_location(
        "kids_gen_art", ROOT / "scripts" / "kids_ui" / "gen_art.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def header_defines(path):
    """Map of #define names to their values, with string literals joined."""
    text = path.read_text(encoding="utf-8").replace("\\\r\n", " ").replace("\\\n", " ")
    defines = {}
    for name, value in re.findall(r"^#define\s+(\w+)\s+(.*)$", text, re.MULTILINE):
        literals = re.findall(r'"((?:[^"\\]|\\.)*)"', value)
        defines[name] = "".join(literals) if literals else value.strip()
    return defines


def charset(name):
    path = CHARSETS / f"{name}.json"
    if not path.exists():
        return None
    return set(json.loads(path.read_text(encoding="utf-8"))["codepoints"])


class KidsUiTest(unittest.TestCase):
    def test_generated_art_is_up_to_date(self):
        try:
            import numpy  # noqa: F401
            import PIL
        except ImportError:
            self.skipTest("gen_art.py needs Pillow and numpy")
        committed = {
            name: (BOARD / name).read_bytes().decode("utf-8").replace("\r\n", "\n")
            for name in ("kids_art.h", "kids_art.c")
        }
        note = f"// Rendered with Pillow {PIL.__version__}."
        if note not in committed["kids_art.h"]:
            self.skipTest(f"the art was rendered with another Pillow version than {PIL.__version__}")
        header, source, _, _, _ = load_generator().generate()
        for name, expected in (("kids_art.h", header), ("kids_art.c", source)):
            self.assertEqual(committed[name], expected,
                             f"{name} is stale: run scripts/kids_ui/gen_art.py")

    def test_patches_lie_inside_the_stage(self):
        defines = header_defines(BOARD / "kids_art.h")
        stage_w = int(defines["KIDS_ART_STAGE_W"])
        stage_h = int(defines["KIDS_ART_STAGE_H"])
        for patch in ("EYES", "MOUTH", "FX_LEFT", "FX_RIGHT"):
            x, y, w, h = (int(defines[f"KIDS_ART_{patch}_{k}"]) for k in "XYWH")
            with self.subTest(patch=patch):
                self.assertGreater(w, 0)
                self.assertGreater(h, 0)
                self.assertGreaterEqual(x, 0)
                self.assertGreaterEqual(y, 0)
                self.assertLessEqual(x + w, stage_w)
                self.assertLessEqual(y + h, stage_h)

    def test_fixed_strings_render_with_the_builtin_font(self):
        basic = charset("basic")
        if basic is None:
            self.skipTest("managed_components/78__xiaozhi-fonts is not downloaded")
        texts = {k: v for k, v in header_defines(BOARD / "kids_strings.h").items()
                 if k.startswith("KIDS_TEXT_")}
        self.assertTrue(texts)
        for name, text in texts.items():
            missing = [c for c in text if ord(c) > 0x7F and ord(c) not in basic]
            self.assertEqual(missing, [], f"{name} uses characters missing from the basic font")

    def test_demo_strings_render_with_the_asset_font(self):
        common = charset("common")
        if common is None:
            self.skipTest("managed_components/78__xiaozhi-fonts is not downloaded")
        texts = {k: v for k, v in header_defines(BOARD / "kids_strings.h").items()
                 if k.startswith("KIDS_DEMO_")}
        self.assertTrue(texts)
        for name, text in texts.items():
            missing = [c for c in text if ord(c) > 0x7F and ord(c) not in common]
            self.assertEqual(missing, [], f"{name} uses characters missing from the common font")

    def test_release_build_only_adds_idle_deep_sleep(self):
        config = json.loads((BOARD / "config.kids.json").read_text(encoding="utf-8"))
        builds = {build["name"]: build for build in config["builds"]}
        test, release = builds["ai-passport-kids"], builds["ai-passport-kids-release"]
        symbol = "CONFIG_AI_PASSPORT_IDLE_DEEP_SLEEP"
        self.assertEqual([o for o in test["sdkconfig_append"] if o.startswith(symbol + "=")], [],
                         "the test firmware must stay awake")
        expected = dict(test, name="ai-passport-kids-release",
                        sdkconfig_append=test["sdkconfig_append"] + [symbol + "=y"])
        self.assertEqual(release, expected, "the release build drifted from the test build")


if __name__ == "__main__":
    unittest.main()
