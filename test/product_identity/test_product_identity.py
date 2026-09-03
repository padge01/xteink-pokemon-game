#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


class ProductIdentityTest(unittest.TestCase):
    def test_crossink_remains_the_default_product_name(self) -> None:
        app_version = (REPO_ROOT / "lib" / "AppVersion" / "AppVersion.h").read_text(
            encoding="utf-8"
        )

        self.assertIn("#ifndef CROSSINK_PRODUCT_NAME", app_version)
        self.assertIn('#define CROSSINK_PRODUCT_NAME "CrossInk"', app_version)

    def test_pokemon_x3_overrides_the_product_name(self) -> None:
        platformio = (REPO_ROOT / "platformio.ini").read_text(encoding="utf-8")
        match = re.search(
            r"^\[env:pokemon-x3\]\s*$\n(?P<section>.*?)(?=^\[|\Z)",
            platformio,
            flags=re.MULTILINE | re.DOTALL,
        )

        self.assertIsNotNone(match, "platformio.ini is missing [env:pokemon-x3]")
        self.assertIn(
            r'-DCROSSINK_PRODUCT_NAME=\"Xteink Pokemon\"', match.group("section")
        )

    def test_settings_footer_uses_the_configured_product_name(self) -> None:
        settings = (
            REPO_ROOT / "src" / "activities" / "settings" / "SettingsActivity.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn(
            'const std::string label = CROSSINK_PRODUCT_NAME " " CROSSINK_VERSION;',
            settings,
        )
        self.assertNotIn(
            'const std::string label = "CrossInk " CROSSINK_VERSION;', settings
        )


if __name__ == "__main__":
    unittest.main()
