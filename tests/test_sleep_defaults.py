"""Validate opt-in CLI defaults without invoking compilers or dependencies."""
import argparse
import contextlib
import io
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from sleep_defaults import add_sleep_defaults_arguments, sleep_default_flags, sleep_defaults_record

class SleepDefaults(unittest.TestCase):
    def parse(self,*args):
        parser=argparse.ArgumentParser()
        add_sleep_defaults_arguments(parser)
        return parser.parse_args(args)

    def test_omitted_and_independent_flags(self):
        args=self.parse()
        self.assertEqual(sleep_default_flags(args),[])
        self.assertEqual(sleep_defaults_record(args),{'mode':'hybrid','idle_seconds':60,'scope':'fallback-only','persisted_preferences_take_precedence':True})
        self.assertEqual(sleep_default_flags(self.parse('--default-sleep-mode','deep')),['-DWATCH_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_DEEP'])
        self.assertEqual(sleep_default_flags(self.parse('--default-idle-seconds','5')),['-DWATCH_DEFAULT_IDLE_SECONDS=5u'])

    def test_choices_and_bounds(self):
        for mode in ('light','deep','hybrid'):
            for seconds in (5,10,60,3600):
                args=self.parse('--default-sleep-mode',mode,'--default-idle-seconds',str(seconds))
                self.assertEqual(sleep_default_flags(args),['-DWATCH_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_'+mode.upper(),f'-DWATCH_DEFAULT_IDLE_SECONDS={seconds}u'])
                self.assertEqual(sleep_defaults_record(args)['mode'],mode)
                self.assertEqual(sleep_defaults_record(args)['idle_seconds'],seconds)
        for flag,values in (('--default-sleep-mode',('none','DEEP','1')),('--default-idle-seconds',('-1','0','4','3601','99999999999999','1.5','nan'))):
            for value in values:
                with self.subTest(flag=flag,value=value),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                    self.parse(flag,value)

if __name__=='__main__':unittest.main()
