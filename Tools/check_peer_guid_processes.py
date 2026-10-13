"""Check parallel-process identities without a renderer or game content."""
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', required=True)
args = parser.parse_args()
processes = []
try:
    for _ in range(4):
        processes.append(subprocess.Popen([args.exe, '--dump'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
    all_ids = set()
    for process in processes:
        output, error = process.communicate(timeout=30)
        if process.returncode != 0:
            raise RuntimeError('Identity helper failed: ' + error)
        ids = [int(value) for value in output.splitlines()]
        if len(ids) != 1000 or len(set(ids)) != 1000 or any(value in (0, (1 << 64)-1) for value in ids) or all_ids.intersection(ids):
            raise RuntimeError('Invalid or duplicate cross-process identity.')
        all_ids.update(ids)
    print('peer_guid_processes=passed processes=4 ids=' + str(len(all_ids)))
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
        process.wait()
