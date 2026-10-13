#!/usr/bin/env python3
"""Portable dedicated-server startup/config/console check; no display or client required."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--game-dir', required=True, type=Path)
    parser.add_argument('--output-prefix', default='PortableServerSmoke')
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,48}', args.output_prefix) or args.timeout <= 0:
        parser.error('Use a simple output prefix and a positive timeout.')
    game = args.game_dir.resolve(strict=True)
    binary = args.exe.resolve(strict=True)
    root = game / 'ScreenShots' / args.output_prefix
    logs = root / 'Сервер_Ёжик'
    service_logs = root / 'ServiceMode'
    for directory in (game / 'ScreenShots', root, logs, service_logs):
        if directory.is_symlink() or directory.resolve() != directory:
            raise RuntimeError('Diagnostic paths must not be symlinks.')
        directory.mkdir(exist_ok=True)
    # Clear only this runner's known outputs, never other workspace files.
    for directory, names in ((root, ('сервер.ini', 'stdout.log', 'stderr.log', 'invalid-config.log', 'service-stdout.log', 'service-stderr.log', 'result.json')),
                             (logs, ('Server.log', 'ServerFailure.log', 'WorldStateServer.log', 'LogLoading.txt', 'LogConsole.txt')),
                             (service_logs, ('Server.log', 'ServerFailure.log', 'WorldStateServer.log', 'LogLoading.txt', 'LogConsole.txt'))):
        for name in names:
            path = directory / name
            if path.exists() or path.is_symlink():
                path.unlink()
    config = root / 'сервер.ini'
    config.write_text('[Server]\nListenAddress = 0.0.0.0\nPort = 1\nPlayers = 2\n'
                      'Name = Сервер config Ж\nActivityClass = GATutorial\nActivity = Tutorial Mission\n'
                      'Scene = Tutorial Bunker\nIncludeMods = false\nLogDirectory = "' + str(logs) + '"\n', encoding='utf-8')
    environment = os.environ.copy()
    environment.pop('DISPLAY', None)
    environment.pop('WAYLAND_DISPLAY', None)
    environment['SDL_VIDEODRIVER'] = 'invalid-driver-for-headless-test'
    environment['SDL_AUDIODRIVER'] = 'invalid-driver-for-headless-test'
    creation_flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    started = time.monotonic()
    process = None
    result = {'result': 'failed', 'platform': os.name}
    server_log = logs / 'Server.log'

    def check():
        if time.monotonic() - started >= args.timeout:
            raise RuntimeError('Server smoke deadline expired.')
        if process.poll() is not None:
            raise RuntimeError('Server exited unexpectedly: ' + str(process.returncode))

    def read_log():
        return server_log.read_text(encoding='utf-8') if server_log.exists() else ''

    try:
        with (root / 'invalid-config.log').open('wb') as invalid_log:
            invalid = subprocess.run([str(binary), '--players', '0'], cwd=game, env=environment,
                                     stdout=invalid_log, stderr=subprocess.STDOUT, timeout=args.timeout,
                                     creationflags=creation_flags, check=False)
        invalid_text = (root / 'invalid-config.log').read_text(encoding='utf-8')
        if invalid.returncode == 0 or 'Players' not in invalid_text:
            raise RuntimeError('Invalid player limit was not rejected.')
        with (root / 'stdout.log').open('wb') as stdout, (root / 'stderr.log').open('wb') as stderr:
            process = subprocess.Popen([str(binary), '--config', str(config), '--listen', '127.0.0.1',
                                        '--port', '0', '--name', 'Сервер CLI Ёжик'], cwd=game, env=environment,
                                       stdin=subprocess.PIPE, stdout=stdout, stderr=stderr, creationflags=creation_flags)
            while True:
                check()
                text = read_log()
                ready = re.search(r'INFO ready port=(\d+) video_initialized=0', text)
                if ready:
                    port = int(ready.group(1))
                    if not 0 < port <= 65535 or port == 1 or 'INFO starting name=Сервер CLI Ёжик' not in text:
                        raise RuntimeError('UTF-8 config/CLI override check failed.')
                    break
                time.sleep(0.1)
            first_tick = None
            while True:
                check()
                process.stdin.write(b'status\n')
                process.stdin.flush()
                time.sleep(0.2)
                statuses = re.findall(r'INFO status tick=(\d+) clients=0 actors=(\d+)', read_log())
                if not statuses:
                    continue
                tick, actors = map(int, statuses[-1])
                if actors == 0:
                    raise RuntimeError('Tutorial simulation has no actors.')
                if first_tick is None:
                    first_tick = tick
                if tick - first_tick >= 120:
                    break
            process.stdin.write(b'quit\n')
            process.stdin.flush()
            process.wait(timeout=max(1, args.timeout - (time.monotonic() - started)))
            if process.returncode != 0 or 'success=1' not in read_log():
                raise RuntimeError('Server shutdown failed.')
            result.update(result='passed', port=port, ticks_observed=tick-first_tick, actors=actors,
                          video_initialized=0, utf8_config_and_cli=True, invalid_config_rejected=True,
                          service_sigterm=None)
        if os.name == 'posix':
            process.stdin.close()
            server_log = service_logs / 'Server.log'
            with (root / 'service-stdout.log').open('wb') as stdout, (root / 'service-stderr.log').open('wb') as stderr:
                process = subprocess.Popen([str(binary), '--config', str(config), '--port', '0',
                                            '--log-directory', str(service_logs)], cwd=game, env=environment,
                                           stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr)
                while 'video_initialized=0' not in read_log():
                    check(); time.sleep(0.1)
                time.sleep(0.5)
                check()  # Closed stdin must not stop a systemd/Docker-style server.
                process.terminate()  # POSIX SIGTERM, handled by ServerConsole.
                process.wait(timeout=max(1, args.timeout-(time.monotonic()-started)))
                if process.returncode != 0 or 'success=1' not in read_log():
                    raise RuntimeError('Service-mode SIGTERM shutdown failed.')
                result['service_sigterm'] = True
    except Exception as error:
        result.update(result='failed', error=str(error))
        raise
    finally:
        if process is not None:
            if process.poll() is None:
                try:
                    if process.stdin is not None and not process.stdin.closed:
                        process.stdin.write(b'quit\n'); process.stdin.flush()
                    else:
                        process.terminate()
                    process.wait(timeout=5)
                except (BrokenPipeError, OSError, subprocess.TimeoutExpired):
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait()
            if process.stdin is not None:
                process.stdin.close()
        result['elapsed_seconds'] = round(time.monotonic()-started, 2)
        (root / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
