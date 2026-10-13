#!/usr/bin/env python3
"""Verify the effective Tracy macro in Meson's compilation database."""
import argparse
import json
from pathlib import Path
import shlex
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-commands', required=True, type=Path)
    parser.add_argument('--expect', choices=('enabled', 'disabled'), required=True)
    args = parser.parse_args()
    commands = json.loads(args.compile_commands.read_text(encoding='utf-8'))
    violations = []
    clients = 0
    for record in commands:
        source = record['file'].replace('\\', '/')
        # Only split for flags; paths themselves remain opaque.
        flags = record.get('arguments')
        if flags is None:
            flags = shlex.split(record['command'], posix=False)
        enabled = False
        normalized = [flag.strip("\"'") for flag in flags]
        for index, flag in enumerate(normalized):
            if flag in ('-D', '/D', '-U', '/U') and index + 1 < len(normalized):
                flag += normalized[index + 1]
            if re.fullmatch(r'(?:-D|/D)TRACY_ENABLE(?:=.*)?', flag):
                enabled = True
            elif flag in ('-UTRACY_ENABLE', '/UTRACY_ENABLE'):
                enabled = False
        client = source.rsplit('/', 1)[-1] == 'TracyClient.cpp'
        clients += client
        if (args.expect == 'disabled' and enabled) or (args.expect == 'enabled' and client and not enabled):
            violations.append(source)
    if clients == 0:
        raise SystemExit('TracyClient.cpp was not found; the database cannot prove this option.')
    if violations:
        raise SystemExit('Tracy option mismatch: ' + ', '.join(violations[:10]))
    print(json.dumps({'result': 'passed', 'expected': args.expect, 'commands_checked': len(commands), 'tracy_clients': clients}))


if __name__ == '__main__':
    main()
