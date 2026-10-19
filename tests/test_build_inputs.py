"""The builder rejects declared pins that do not match actual input bytes."""
import ast
import json
import importlib.util
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('build_inputs',ROOT/'scripts/build_inputs.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class Inputs(unittest.TestCase):
    def test_home_version_checkpoint(self):
        tree=ast.parse((ROOT/'scripts/build.py').read_text())
        versions=[node.value for node in ast.walk(tree) if isinstance(node,ast.Assign)
                  and any(ast.unparse(target)=="manifest['version']" for target in node.targets)]
        self.assertEqual(len(versions),1)
        version=versions[0]
        self.assertIsInstance(version,ast.IfExp)
        self.assertEqual(ast.unparse(version.test),'a.serial_transfer_mode')
        self.assertEqual(ast.literal_eval(version.body),'0.20.4')
        version=version.orelse
        self.assertIsInstance(version,ast.IfExp)
        self.assertEqual(ast.unparse(version.test),'a.automatic_provision')
        self.assertEqual(ast.literal_eval(version.body),'0.20.3')
        self.assertEqual(ast.literal_eval(version.orelse),'0.19.3')
        self.assertEqual(json.loads((ROOT/'apps/clock/default.json').read_text())['version'],'0.19.3')
        self.assertEqual(json.loads((ROOT/'apps/clock/clock.json').read_text())['version'],'0.15.0')
        self.assertEqual(json.loads((ROOT/'apps/clock/manifest.json').read_text())['version'],'0.8.0')

    def test_pinned_bytes_and_rejection(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'include').mkdir();(root/'include/api.h').write_text('original\n')
            (root/'unrelated.c').write_text('outside\n')
            env={**os.environ,'GIT_AUTHOR_NAME':'Build test','GIT_AUTHOR_EMAIL':'test@example.invalid',
                 'GIT_COMMITTER_NAME':'Build test','GIT_COMMITTER_EMAIL':'test@example.invalid'}
            def git(*args):return subprocess.check_output(['git','-C',str(root),*args],env=env,stderr=subprocess.DEVNULL).decode().strip()
            git('init');git('add','.');git('commit','-m','Fixture');head=git('rev-parse','HEAD')
            result=module.verify_dependency(root,head,['include'])
            self.assertEqual(set(result['files_sha256']),{'include/api.h'})
            (root/'unrelated.c').write_text('Allowed unrelated work\n')
            self.assertEqual(result,module.verify_dependency(root,head,['include']))
            with self.assertRaisesRegex(RuntimeError,'revision mismatch'):
                module.verify_dependency(root,'0'*40,['include'])
            (root/'include/api.h').write_text('changed\n')
            with self.assertRaisesRegex(RuntimeError,'Dirty'):
                module.verify_dependency(root,head,['include'])
            # Even an index flag hiding the worktree change cannot attest bad bytes.
            git('update-index','--assume-unchanged','include/api.h')
            with self.assertRaisesRegex(RuntimeError,'bytes differ'):
                module.verify_dependency(root,head,['include'])
            (root/'include/api.h').write_text('original\n');(root/'include/shadow.h').write_text('untracked\n')
            with self.assertRaisesRegex(RuntimeError,'Dirty'):
                module.verify_dependency(root,head,['include'])

if __name__=='__main__':unittest.main()
