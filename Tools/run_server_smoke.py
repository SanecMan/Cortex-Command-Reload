#!/usr/bin/env python3
"""Portable dedicated-server startup/config/console check; no display or client required."""
import argparse
import contextlib
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
    parser.add_argument('--self-test', action='store_true', help='Run the server binary device-free diagnostics before startup checks.')
    parser.add_argument('--probe-clients', action='store_true', help='Connect two separate device-free client processes to the live server.')
    parser.add_argument('--screen-text', action='store_true', help='Require server HUD messages in the separate client probes.')
    args = parser.parse_args()
    if args.screen_text and not args.probe_clients:
        parser.error('--screen-text requires --probe-clients.')
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
    for directory, names in ((root, ('сервер.ini', 'stdout.log', 'stderr.log', 'invalid-config.log', 'self-test-stdout.log', 'client-0.log', 'client-1.log', 'service-stdout.log', 'service-stderr.log', 'result.json')),
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
        if args.self_test:
            test_name = args.output_prefix[:40] + 'SelfTest'
            test_directory = game / 'ScreenShots' / test_name
            if test_directory.is_symlink() or test_directory.resolve() != test_directory:
                raise RuntimeError('Self-test output must not be a symlink.')
            with (root / 'self-test-stdout.log').open('wb') as output:
                probe = subprocess.run([str(binary), '--self-test', '-debug-run-output', test_name], cwd=game, env=environment,
                                       stdout=output, stderr=subprocess.STDOUT, timeout=args.timeout,
                                       creationflags=creation_flags, check=False)
            report = (test_directory / 'DebugRun.log').read_text(encoding='utf-8')
            markers = ('result=passed', 'gpu_enabled=0', 'world_state_host_session_smoke=passed',
                       'clients_received=4', 'live_world_received=1', 'server_alive_after_disconnect=1',
                       'headless_sound_length_smoke=passed', 'playback=1')
            if probe.returncode != 0 or not all(marker in report for marker in markers):
                raise RuntimeError('Dedicated server self-test failed; inspect its DebugRun.log.')
            result['self_test'] = True
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
            if args.probe_clients:
                if args.screen_text:
                    process.stdin.write(b'debug-screen-text\n'); process.stdin.flush()
                probes = []
                with contextlib.ExitStack() as streams:
                    try:
                        for index in range(2):
                            output = streams.enter_context((root / f'client-{index}.log').open('wb'))
                            probes.append(subprocess.Popen([str(binary), '--probe-client', '127.0.0.1', str(port)] + (['--screen-text'] if args.screen_text else []),
                                cwd=game, env=environment, stdout=output, stderr=subprocess.STDOUT,
                                creationflags=creation_flags))
                        observed_two = False
                        while any(probe.poll() is None for probe in probes):
                            check()
                            process.stdin.write(b'status\n'); process.stdin.flush()
                            time.sleep(0.1)
                            observed_two |= bool(re.search(r'INFO status tick=\d+ clients=2 actors=\d+', read_log()))
                        if not observed_two or any(probe.returncode != 0 for probe in probes):
                            raise RuntimeError('Separate client connection check failed; inspect client logs.')
                    finally:
                        for probe in probes:
                            if probe.poll() is None:
                                probe.terminate()
                                try:
                                    probe.wait(timeout=5)
                                except subprocess.TimeoutExpired:
                                    probe.kill(); probe.wait()
                for index in range(2):
                    report = (root / f'client-{index}.log').read_text(encoding='utf-8')
                    if not re.search(r'result=passed snapshots=\d+ objects=[1-9]\d* tick_delta=\d+ input_queued=[1-9]\d*', report):
                        raise RuntimeError('Separate client did not receive an advancing populated world.')
                    if args.screen_text and 'hud_received=1' not in report:
                        raise RuntimeError('Separate client did not receive a server HUD message.')
                network_log = (logs / 'WorldStateServer.log').read_text(encoding='utf-8')
                if not re.search(r'INFO: applied [1-9]\d* client input commands to player slot', network_log):
                    raise RuntimeError('Server did not apply neutral input from a probe client.')
                result['separate_clients'] = 2
                result['probe_input_applied'] = True
                if args.screen_text:
                    result['screen_text_received'] = True
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
