"""Generic opt-in Home fallback policy. Saved valid preferences always win."""
import argparse


def idle_seconds(value):
    try:
        seconds = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError('idle seconds must be an integer from 5 to 3600') from error
    if not 5 <= seconds <= 3600:
        raise argparse.ArgumentTypeError('idle seconds must be from 5 to 3600')
    return seconds


def add_sleep_defaults_arguments(parser):
    parser.add_argument('--default-sleep-mode', choices=['deep', 'light', 'hybrid'],
                        help='Home sleep fallback only; valid saved preferences win (otherwise hybrid)')
    parser.add_argument('--default-idle-seconds', type=idle_seconds,
                        help='Home idle fallback, 5–3600 seconds; valid saved preferences win (otherwise 60)')


def sleep_default_flags(args):
    flags = []
    if args.default_sleep_mode is not None:
        flags.append('-DWATCH_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_' + args.default_sleep_mode.upper())
    if args.default_idle_seconds is not None:
        flags.append('-DWATCH_DEFAULT_IDLE_SECONDS=' + str(args.default_idle_seconds) + 'u')
    return flags


def sleep_defaults_record(args):
    return {'mode': args.default_sleep_mode or 'hybrid',
            'idle_seconds': args.default_idle_seconds if args.default_idle_seconds is not None else 60,
            'scope': 'fallback-only', 'persisted_preferences_take_precedence': True}
