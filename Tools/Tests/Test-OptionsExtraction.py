"""Regression checks for lexical ownership and fresh-source documentation gates."""
import json
from pathlib import Path
import runpy
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
EXTRACT = runpy.run_path(str(TOOLS / 'Extract-Options.py'))


class OptionsExtractionTests(unittest.TestCase):
    def test_nested_body_closure_restores_outer_owner(self):
        source = '''
        class UOuter {
          struct FInner { UPROPERTY() int32 Inner = 0; };
          // struct FComment { UPROPERTY() int32 Fake; };
          const char* Literal = "struct FFalse { }";
          const char* Raw = R"test(struct FFake { })test";
          UPROPERTY() int32 Outer = 0;
          class UForward;
          void Function(class UParameter* Arg);
          UPROPERTY() int32 AfterForward = 0;
        };
        struct FNext { UPROPERTY() bool Ready = false; };
        '''
        owners = [EXTRACT['enclosing_class'](source, offset)
                  for _, _, offset in EXTRACT['find_uproperties'](source)]
        self.assertEqual(owners, ['FInner', 'UOuter', 'UOuter', 'FNext'])

    def test_freshness_rejects_source_edits_even_when_cached_docs_agree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'Source/TerritoryFramework/Public/Test.h'
            source.parent.mkdir(parents=True)
            source.write_text('class UTest { UPROPERTY(EditAnywhere) int32 Count = 1; };')
            inventory = root / 'Tools/options_inventory.json'
            inventory.parent.mkdir()
            rows = EXTRACT['extract_inventory'](str(root))
            inventory.write_text(json.dumps(rows))
            self.assertEqual(EXTRACT['read_fresh_inventory'](str(root)), rows)
            source.write_text(source.read_text().replace('Count = 1', 'Count = 2'))
            with self.assertRaisesRegex(SystemExit, 'Stale options_inventory'):
                EXTRACT['read_fresh_inventory'](str(root))

    def test_current_repository_resolves_all_real_properties(self):
        rows = EXTRACT['extract_inventory']()
        self.assertGreater(len(rows), 0)
        self.assertEqual([r for r in rows if r['class'] == '?'], [])


if __name__ == '__main__':
    unittest.main()
