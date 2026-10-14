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
    parser.add_argument('--objectives', action='store_true', help='Require native UTF-8 objective-point capture in client probes.')
    parser.add_argument('--probe-seconds', type=int, default=3, help='Duration of each client connection (3..3600 seconds).')
    parser.add_argument('--input-silence', action='store_true', help='Pause client input for two seconds every six seconds.')
    parser.add_argument('--activity-class', default='GATutorial')
    parser.add_argument('--activity', default='Tutorial Mission')
    parser.add_argument('--scene', default='Tutorial Bunker')
    parser.add_argument('--require-players', type=int, default=0, help='Require this many distinct active player slots (0..2).')
    parser.add_argument('--restart-with-clients', action='store_true', help='Restart the Activity after both peers connect.')
    parser.add_argument('--require-control', action='store_true', help='Require distinct controlled actors, with or without restart.')
    parser.add_argument('--restart-count', type=int, default=1, help='Number of connected-roster restarts (1..16).')
    parser.add_argument('--disconnect-one-restart', action='store_true', help='Restart again after the first probe disconnects; the remaining probe requires one human.')
    parser.add_argument('--check-deferred-restart', action='store_true', help='Request restart with no peers, then connect a new player and require completion.')
    parser.add_argument('--second-client-delay', type=float, default=0, help='Delay the second peer by 0..30 seconds for late admission checks.')
    parser.add_argument('--construction-check', action='store_true', help='Check construction response routing and replay/scene/phase guards.')
    parser.add_argument('--brain-start', action='store_true', help='Place brains through two clients, reject out-of-bounds replacement and start the native match.')
    args = parser.parse_args()
    if (args.screen_text or args.objectives) and not args.probe_clients:
        parser.error('--screen-text/--objectives require --probe-clients.')
    if not 3 <= args.probe_seconds <= 3600 or (args.input_silence and (not args.probe_clients or args.probe_seconds < 12)):
        parser.error('Use probe duration 3..3600; input silence requires clients and at least 12 seconds.')
    if not 0 <= args.require_players <= 2 or (args.require_players and not args.probe_clients):
        parser.error('Require players must be 0..2 and needs client probes.')
    if (args.restart_with_clients or args.require_control) and not args.probe_clients:
        parser.error('Restart and control checks require clients.')
    if not 1 <= args.restart_count <= 16 or (args.restart_count > 1 and (not args.restart_with_clients or not args.require_control or args.probe_seconds < args.restart_count * 3)):
        parser.error('Repeated restarts need control checks and at least three probe seconds per restart.')
    if args.disconnect_one_restart and (not args.restart_with_clients or not args.require_control or args.restart_count != 1 or args.input_silence or args.probe_seconds < 12):
        parser.error('Disconnect/restart needs control checks, one initial restart, no silence and at least 12 seconds.')
    if args.check_deferred_restart and not args.probe_clients:
        parser.error('Deferred restart check requires client probes.')
    if args.construction_check and (not args.probe_clients or args.restart_with_clients or args.input_silence):
        parser.error('Construction checks need probes and must run separately from restart/silence fixtures.')
    if args.brain_start:
        if not args.probe_clients or args.construction_check or args.restart_with_clients or args.input_silence or args.second_client_delay:
            parser.error('Brain start needs two concurrent probes and a separate editing fixture.')
        args.require_control = True
        args.require_players = 2
    if not 0 <= args.second_client_delay <= 30 or (args.second_client_delay and (not args.probe_clients or args.probe_seconds < args.second_client_delay + 3)):
        parser.error('Second client delay needs probes lasting at least delay + 3 seconds.')
    if args.second_client_delay and args.disconnect_one_restart:
        parser.error('Run delayed admission separately from the short first-peer disconnect fixture.')
    if any(not value or '\n' in value or '\r' in value for value in (args.activity_class, args.activity, args.scene)):
        parser.error('Activity and scene must be nonempty single-line preset names.')
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
    for directory, names in ((root, ('сервер.ini', 'stdout.log', 'stderr.log', 'invalid-config.log', 'self-test-stdout.log', 'client-0.log', 'client-1.log', 'deferred-client.log', 'service-stdout.log', 'service-stderr.log', 'result.json')),
                             (logs, ('Server.log', 'ServerFailure.log', 'WorldStateServer.log', 'LogLoading.txt', 'LogConsole.txt')),
                             (service_logs, ('Server.log', 'ServerFailure.log', 'WorldStateServer.log', 'LogLoading.txt', 'LogConsole.txt'))):
        for name in names:
            path = directory / name
            if path.exists() or path.is_symlink():
                path.unlink()
    config = root / 'сервер.ini'
    config.write_text('[Server]\nListenAddress = 0.0.0.0\nPort = 1\nPlayers = 2\n'
                      'Name = Сервер config Ж\nActivityClass = ' + args.activity_class + '\nActivity = ' + args.activity + '\n'
                      'Scene = ' + args.scene + '\nIncludeMods = false\nLogDirectory = "' + str(logs) + '"\n', encoding='utf-8')
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
                       'headless_sound_length_smoke=passed', 'headless_sound_capture=passed', 'headless_sound_replica=passed',
                       'headless_replica_spawn=passed', 'playback=1')
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
                if args.objectives:
                    process.stdin.write(b'debug-objectives\n'); process.stdin.flush()
                probes = []
                with contextlib.ExitStack() as streams:
                    try:
                        for index in range(2):
                            if index == 1 and args.second_client_delay:
                                deadline = time.monotonic() + args.second_client_delay
                                while time.monotonic() < deadline:
                                    check()
                                    if probes[0].poll() is not None:
                                        raise RuntimeError('First peer exited before the delayed join.')
                                    time.sleep(0.1)
                            output = streams.enter_context((root / f'client-{index}.log').open('wb'))
                            duration = 3 if args.disconnect_one_restart and index == 0 else args.probe_seconds
                            minimum_revision = (3 if args.disconnect_one_restart and index == 1 else args.restart_count + 1) if args.restart_with_clients else 1
                            control_options = ['--require-control', '--minimum-revision', str(minimum_revision)] if args.require_control else []
                            if args.disconnect_one_restart:
                                control_options += ['--expected-humans', str(2 if index == 0 else 1)]
                            probes.append(subprocess.Popen([str(binary), '--probe-client', '127.0.0.1', str(port), '--probe-seconds', str(duration)] + (['--screen-text'] if args.screen_text else []) + (['--objectives'] if args.objectives else []) + control_options + (['--input-silence'] if args.input_silence else []) + (['--construction-check'] if args.construction_check else []) + (['--brain-start'] if args.brain_start else []),
                                cwd=game, env=environment, stdout=output, stderr=subprocess.STDOUT,
                                creationflags=creation_flags))
                        observed_two = False
                        restart_sent = False
                        restarts_sent = 0
                        last_restart = 0.0
                        subset_restart_sent = False
                        while any(probe.poll() is None for probe in probes):
                            check()
                            if args.objectives:
                                process.stdin.write(b'debug-objectives\n'); process.stdin.flush()
                            process.stdin.write(b'status\n'); process.stdin.flush()
                            time.sleep(0.1)
                            observed_two |= bool(re.search(r'INFO status tick=\d+ clients=2 actors=\d+', read_log()))
                            if args.restart_with_clients and observed_two and restarts_sent < args.restart_count and time.monotonic() - last_restart >= 3:
                                process.stdin.write(b'restart\n'); process.stdin.flush()
                                restart_sent = True
                                restarts_sent += 1
                                last_restart = time.monotonic()
                            if args.disconnect_one_restart and restart_sent and not subset_restart_sent and probes[0].poll() == 0 and probes[1].poll() is None:
                                statuses = re.findall(r'INFO status tick=\d+ clients=(\d+) actors=\d+', read_log())
                                if statuses and statuses[-1] == '1':
                                    process.stdin.write(b'restart\n'); process.stdin.flush()
                                    subset_restart_sent = True
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
                active_slots = set()
                editing_probes = True
                controlled_actors = set()
                for index in range(2):
                    report = (root / f'client-{index}.log').read_text(encoding='utf-8')
                    object_count_pattern = r'\d+' if args.construction_check and 'activity_state=2 ' in report else r'[1-9]\d*'
                    if not re.search(r'result=passed snapshots=\d+ objects=' + object_count_pattern + r' tick_delta=\d+ input_queued=[1-9]\d*', report):
                        raise RuntimeError('Separate client did not receive an advancing populated world.')
                    if args.screen_text and 'hud_received=1' not in report:
                        raise RuntimeError('Separate client did not receive a server HUD message.')
                    if args.objectives and 'objectives_received=1' not in report:
                        raise RuntimeError('Separate client did not receive the native objective fixture.')
                    if args.construction_check and 'construction_checks=7' not in report:
                        raise RuntimeError('Construction command checks were not verified.')
                    editing_probes &= 'activity_state=2 ' in report
                    if args.brain_start and ('brain_start=1' not in report or 'activity_state=4 ' not in report):
                        raise RuntimeError('Native brain placement/start was not verified.')
                    silence_match = re.search(r'silence_windows=(\d+)', report)
                    if args.input_silence and (not silence_match or int(silence_match.group(1)) < 2):
                        raise RuntimeError('Client did not exercise repeated input silence.')
                    assignment = re.search(r'assigned_slot=(-?\d+)', report)
                    if assignment and int(assignment.group(1)) >= 0:
                        active_slots.add(int(assignment.group(1)))
                    control = re.search(r'controlled_actor=(\d+) scene_revision=(\d+)', report)
                    if args.require_control:
                        minimum_revision = (3 if args.disconnect_one_restart and index == 1 else args.restart_count + 1) if args.restart_with_clients else 1
                        if not control or int(control.group(1)) == 0 or int(control.group(2)) < minimum_revision:
                            raise RuntimeError('Client did not receive a controlled actor in the required revision.')
                        if not args.restart_with_clients and int(control.group(2)) != 1:
                            raise RuntimeError('Late admission unexpectedly restarted the Activity.')
                        controlled_actors.add(int(control.group(1)))
                    if args.disconnect_one_restart and f'humans={2 if index == 0 else 1} ' not in report:
                        raise RuntimeError('Disconnected human slot survived restart.')
                if len(active_slots) < args.require_players:
                    raise RuntimeError('Not enough distinct active player assignments.')
                if args.require_control and len(controlled_actors) != 2:
                    raise RuntimeError('Clients do not have distinct controlled actors.')
                if args.restart_with_clients and (not restart_sent or restarts_sent != args.restart_count or read_log().count('INFO activity_restart=completed humans=2') != args.restart_count):
                    raise RuntimeError('Activity restart with the connected roster was not verified.')
                if args.disconnect_one_restart and (not subset_restart_sent or 'INFO activity_restart=completed humans=1' not in read_log()):
                    raise RuntimeError('Single-player roster restart was not verified.')
                network_log = (logs / 'WorldStateServer.log').read_text(encoding='utf-8')
                if not re.search(r'INFO: applied [1-9]\d* client input commands to player slot', network_log):
                    raise RuntimeError('Server did not apply neutral input from a probe client.')
                if args.input_silence and (not active_slots or any(network_log.count(f'neutralized stale network input for player slot {slot}') < 2 for slot in active_slots)):
                    raise RuntimeError('Server did not neutralize each active silent player repeatedly.')
                result['separate_clients'] = 2
                if args.brain_start:
                    result['brain_placement_and_start'] = True
                if args.construction_check:
                    result['construction_checks_per_client'] = 7
                result['probe_seconds'] = args.probe_seconds
                result['second_client_delay'] = args.second_client_delay
                result['input_silence'] = args.input_silence
                result['active_player_slots'] = sorted(active_slots)
                if args.require_control:
                    result['controlled_actors'] = sorted(controlled_actors)
                    result['restarts'] = args.restart_count if args.restart_with_clients else 0
                    if args.disconnect_one_restart:
                        result['disconnected_roster_restart'] = True
                result['probe_input_applied'] = True
                if args.objectives:
                    result['objectives_received'] = True
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
                if actors == 0 and not (args.construction_check and editing_probes):
                    raise RuntimeError('Tutorial simulation has no actors.')
                if first_tick is None:
                    first_tick = tick
                if tick - first_tick >= 120:
                    break
            if args.check_deferred_restart:
                waiting_before = read_log().count('INFO activity_restart=waiting_for_players')
                process.stdin.write(b'restart\n'); process.stdin.flush()
                while read_log().count('INFO activity_restart=waiting_for_players') == waiting_before:
                    check(); time.sleep(0.1)
                minimum_revision = (args.restart_count + 1 if args.restart_with_clients else 1) + (1 if args.disconnect_one_restart else 0) + 1
                with (root / 'deferred-client.log').open('wb') as output:
                    deferred = subprocess.run([str(binary), '--probe-client', '127.0.0.1', str(port), '--require-control',
                        '--minimum-revision', str(minimum_revision), '--expected-humans', '1'], cwd=game, env=environment,
                        stdout=output, stderr=subprocess.STDOUT, creationflags=creation_flags,
                        timeout=max(1, args.timeout - (time.monotonic() - started)))
                deferred_report = (root / 'deferred-client.log').read_text(encoding='utf-8')
                if deferred.returncode != 0 or 'result=passed' not in deferred_report or 'humans=1 ' not in deferred_report:
                    raise RuntimeError('Deferred restart did not admit a single new player.')
                check()
                result['deferred_restart'] = True
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
                with contextlib.suppress(OSError):
                    process.stdin.close()
        result['elapsed_seconds'] = round(time.monotonic()-started, 2)
        (root / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
