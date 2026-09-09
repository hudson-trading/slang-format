#!/usr/bin/env python3
"""Exercise docs publishing against temporary local Git repositories."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


PUBLISH_SCRIPT = Path(__file__).resolve().parents[2] / "scripts/ci/publish-docs.sh"


class PublishDocsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.remote = self.root / "origin.git"
        self.source = self.root / "source"
        self.site = self.root / "site"
        self.site.mkdir()
        (self.site / "index.html").write_text("new docs")
        self.env = {
            **os.environ,
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_TERMINAL_PROMPT": "0",
            "GIT_AUTHOR_NAME": "Docs test",
            "GIT_AUTHOR_EMAIL": "docs@example.com",
            "GIT_COMMITTER_NAME": "Docs test",
            "GIT_COMMITTER_EMAIL": "docs@example.com",
            "RUNNER_TEMP": str(self.root),
        }
        self.git(self.root, "init", "--bare", "--initial-branch=gh-pages", self.remote)
        self.git(self.root, "clone", self.remote, self.source)
        (self.source / "index.html").write_text("old main")
        (self.source / "obsolete.html").write_text("old main page")
        for number in (1, 2):
            preview = self.source / "previews" / f"pr-{number}"
            preview.mkdir(parents=True)
            (preview / "index.html").write_text(f"old preview {number}")
            (preview / "obsolete.html").write_text("old preview page")
        self.git(self.source, "add", ".")
        self.git(self.source, "commit", "-m", "Initial docs")
        self.git(self.source, "push", "origin", "gh-pages")
        self.git(self.source, "checkout", "-b", "main")

    def git(self, directory, *args):
        return subprocess.check_output(
            ["git", "-C", str(directory), *map(str, args)],
            env=self.env,
            stderr=subprocess.STDOUT,
            text=True,
        ).strip()

    def publish(self, destination=".", *, success=True, site=None):
        before = self.git(self.source, "rev-parse", "HEAD")
        result = subprocess.run(
            ["bash", str(PUBLISH_SCRIPT), str(site or self.site), destination],
            cwd=self.source,
            env=self.env,
            capture_output=True,
            text=True,
            timeout=60,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.git(self.source, "rev-parse", "HEAD"), before)
        self.assertEqual(self.git(self.source, "status", "--porcelain"), "")
        self.assertEqual(
            self.git(self.source, "worktree", "list", "--porcelain").count("worktree "),
            1,
        )
        self.assertEqual((self.site / "index.html").read_text(), "new docs")
        return result

    def remote_file(self, path):
        return self.git(self.remote, "show", f"gh-pages:{path}")

    def test_main_preserves_previews_and_removes_stale_pages(self):
        self.publish()
        self.assertEqual(self.remote_file("index.html"), "new docs")
        for number in (1, 2):
            self.assertEqual(
                self.remote_file(f"previews/pr-{number}/index.html"),
                f"old preview {number}",
            )
        self.assertEqual(self.remote_file(".nojekyll"), "")
        files = self.git(self.remote, "ls-tree", "--name-only", "gh-pages").splitlines()
        self.assertNotIn("obsolete.html", files)
        before = self.git(self.remote, "rev-parse", "gh-pages")
        self.assertIn("already up to date", self.publish().stdout)
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages"), before)

    def test_preview_preserves_main_and_other_previews(self):
        self.publish("previews/pr-1")
        self.assertEqual(self.remote_file("index.html"), "old main")
        self.assertEqual(self.remote_file("obsolete.html"), "old main page")
        self.assertEqual(self.remote_file("previews/pr-1/index.html"), "new docs")
        self.assertEqual(self.remote_file("previews/pr-2/index.html"), "old preview 2")
        files = self.git(
            self.remote, "ls-tree", "--name-only", "gh-pages:previews/pr-1"
        ).splitlines()
        self.assertNotIn("obsolete.html", files)

    def test_concurrent_update_is_preserved_on_retry(self):
        contender = self.root / "contender"
        self.git(self.root, "clone", self.remote, contender)
        (contender / "previews/pr-2/index.html").write_text("concurrent preview")
        self.git(contender, "commit", "-am", "Concurrent deployment")
        winner = self.git(contender, "rev-parse", "HEAD")
        hook = self.source / ".git/hooks/pre-push"
        marker = self.root / "raced"
        hook.write_text(
            "#!/usr/bin/env bash\nset -eu\n"
            f'test "$(git rev-parse --show-toplevel)" = {shlex.quote(str(self.source))}\n'
            f"if [ ! -f {shlex.quote(str(marker))} ]; then\n"
            f"  touch {shlex.quote(str(marker))}\n"
            f"  git -C {shlex.quote(str(contender))} push origin gh-pages\n"
            "fi\n"
        )
        hook.chmod(0o755)
        result = self.publish("previews/pr-1")
        self.assertIn("gh-pages changed during publishing; retrying", result.stdout)
        self.assertEqual(self.remote_file("previews/pr-1/index.html"), "new docs")
        self.assertEqual(
            self.remote_file("previews/pr-2/index.html"), "concurrent preview"
        )
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages^"), winner)

    def test_push_failure_without_a_race_stops_and_cleans_up(self):
        before = self.git(self.remote, "rev-parse", "gh-pages")
        hook = self.source / ".git/hooks/pre-push"
        hook.write_text("#!/usr/bin/env bash\nexit 1\n")
        hook.chmod(0o755)
        result = self.publish(success=False)
        self.assertIn("Push failed without a concurrent gh-pages update", result.stderr)
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages"), before)

    def test_invalid_destinations_and_missing_site_do_not_publish(self):
        before = self.git(self.remote, "rev-parse", "gh-pages")
        for destination in (
            "../outside",
            "previews/pr-0",
            "previews/pr-1/other",
            "previews/pr-x",
        ):
            with self.subTest(destination=destination):
                result = self.publish(destination, success=False)
                self.assertIn("Invalid preview destination", result.stderr)
        missing = self.root / "empty-site"
        missing.mkdir()
        result = self.publish(site=missing, success=False)
        self.assertIn("Missing built documentation", result.stderr)
        self.assertEqual(self.git(self.remote, "rev-parse", "gh-pages"), before)


if __name__ == "__main__":
    unittest.main()
