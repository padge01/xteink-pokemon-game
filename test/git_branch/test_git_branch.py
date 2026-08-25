import importlib.util
import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = REPO_ROOT / "scripts" / "git_branch.py"

spec = importlib.util.spec_from_file_location("crossink_git_branch", SCRIPT_PATH)
git_branch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(git_branch)


class GitBranchTest(unittest.TestCase):
    def test_branch_comes_from_repository_metadata(self):
        branch = git_branch.get_git_branch(str(REPO_ROOT))

        self.assertNotEqual(branch, "unknown")
        self.assertRegex(branch, re.compile(r"^[A-Za-z0-9._-]+$"))

    def test_short_hash_comes_from_repository_metadata(self):
        short_hash = git_branch.get_git_short_hash(str(REPO_ROOT))

        self.assertNotEqual(short_hash, "00000")
        self.assertRegex(short_hash, re.compile(r"^[0-9a-f]{5}$"))


if __name__ == "__main__":
    unittest.main()
