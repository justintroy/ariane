import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "release" / "package_samp_windows.py"
SPEC = importlib.util.spec_from_file_location("package_samp_windows", SCRIPT)
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageOutputSafetyTests(unittest.TestCase):
    def test_engine_is_staged_at_the_metadata_path(self):
        source, target = PACKAGE.engine_candidate(Path("ariane-samp-test.exe"))
        self.assertEqual(source.name, "ariane-samp-test.exe")
        self.assertEqual(target, "ariane.exe")
        self.assertEqual(target, PACKAGE.ENGINE_PACKAGE_NAME)

    def test_resolves_fresh_targets_under_output_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "unused" / ".." / "packages"
            stage, archive = PACKAGE.check_output_targets(output, "candidate")
            self.assertEqual(stage.parent, output.resolve())
            self.assertEqual(archive.parent, output.resolve())

    def test_rejects_resolved_staging_path_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                PACKAGE.check_output_targets(Path(temp) / "packages", "../outside")

    def test_refuses_existing_staging_directory_without_deleting_it(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            stage = output / "candidate"
            stage.mkdir()
            sentinel = stage / "keep.txt"
            sentinel.write_text("preserve", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                PACKAGE.check_output_targets(output, "candidate")
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserve")

    def test_refuses_existing_archive(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            archive = output / "candidate.zip"
            archive.write_bytes(b"keep")
            with self.assertRaises(FileExistsError):
                PACKAGE.check_output_targets(output, "candidate")
            self.assertEqual(archive.read_bytes(), b"keep")

    def test_install_note_explains_flat_copy_and_package_scope(self):
        with tempfile.TemporaryDirectory() as temp:
            note = Path(temp) / "INSTALL.txt"
            PACKAGE.write_install_note(note, has_wheel=False)
            content = note.read_text(encoding="utf-8")
            self.assertIn("copy the executable and fonts folder", content)
            self.assertIn("GTA San Andreas installation", content)
            self.assertIn("preserve it and rename this candidate", content)
            self.assertIn("not release-validated", content)
            self.assertIn("not been validated by installing", content)
            self.assertIn("Python agent CLI is not included", content)

    def test_rejects_sensitive_name_in_package_subdirectory(self):
        reason = PACKAGE.check_forbidden_file(Path("samples/runtime-env.json/map.pwn"))
        self.assertIsNotNone(reason)


if __name__ == "__main__":
    unittest.main()
