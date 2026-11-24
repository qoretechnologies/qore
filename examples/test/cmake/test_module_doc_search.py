#!/usr/bin/env python3
"""Check the per-result search filter used by the user-module documentation targets."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


PRUNE = Path(__file__).resolve().parents[3] / "doxygen/prune-module-search.py"


class ModuleDocSearchTest(unittest.TestCase):
    def test_mixed_results_diagram_links_and_generated_listings(self):
        with tempfile.TemporaryDirectory() as root:
            html = Path(root) / "App/html"
            search = html / "search"
            search.mkdir(parents=True)
            (html / "index.html").write_text(
                '<a href="../../Used/html/class_a.html">A</a><code>../../Unused/html/x.html</code>')
            (html / "class_a.svg").write_text('<a xlink:href="../../Svg/html/base.html"/>')
            (html / "class_b.map").write_text('<area href="/build/native/Map.tag$class_b.html"/>')
            (html / "namespaces.html").write_text('<a href="../../Unused/html/namespace_u.html">U</a>')
            (html / "functions_func_a.html").write_text('<a href="../../Unused/html/a.html">A</a>')

            def row(key, urls):
                targets = ",".join(f"['{url}',1,'scope']" for url in urls)
                return f"  ['{key}',['Overload\\'s name',{targets}]]"

            def shard(rows):
                return "var searchData=\n[\n" + ",\n".join(rows) + "\n];\n"

            unused = "../../../Unused/html/x.html"
            keep = ["../local.html", "../../../../lang/html/x.html", "https://example.org/x",
                    "../../../Used/html/unmentioned.html", "../../../Svg/html/x.html",
                    "../../../Map/html/x.html"]
            mixed = search / "all_a.js"
            mixed.write_text(shard([row("removed", [unused]), row("mixed", [unused, *keep, unused])]))
            empty = search / "classes_b.js"
            empty.write_text(shard([row("removed", [unused])]))
            # An unrecognized format must survive unchanged, even if an earlier row matched.
            other = search / "functions_c.js"
            original = shard([row("removed", [unused]), f"  ['new',['layout','{unused}']]"])
            other.write_text(original)
            for _ in range(2):
                subprocess.run([sys.executable, "-I", str(PRUNE), str(html)], check=True)
                self.assertEqual(shard([row("mixed", keep)]), mixed.read_text())
                self.assertEqual(shard([]), empty.read_text())
                self.assertEqual(original, other.read_text())


if __name__ == "__main__":
    unittest.main()
