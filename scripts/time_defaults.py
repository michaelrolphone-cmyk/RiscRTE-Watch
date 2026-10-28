"""Explicit local-calendar seed for an unset native clock, never inferred time."""
import argparse
from datetime import datetime

def local_time(value):
    try:
        parsed = datetime.strptime(value, '%Y-%m-%dT%H:%M:%S')
    except ValueError as error:
        raise argparse.ArgumentTypeError('use YYYY-MM-DDTHH:MM:SS') from error
    if parsed.strftime('%Y-%m-%dT%H:%M:%S') != value or not 2000 <= parsed.year <= 2037:
        raise argparse.ArgumentTypeError('local default must be an exact calendar in 2000–2037')
    return value

def time_default_flags(value):
    if value is None:
        return []
    parsed = datetime.strptime(local_time(value), '%Y-%m-%dT%H:%M:%S')
    fields = (parsed.year, parsed.month, parsed.day, parsed.hour, parsed.minute, parsed.second)
    return ['-DWATCH_UNSET_TIME_LOCAL=' + ','.join(map(str, fields))]
