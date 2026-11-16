"""Run explicitly: python3 tests/soak_test.py build/linux-process-monitor [seconds]."""
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import time
from pathlib import Path

app = sys.argv[1]
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
master, slave = pty.openpty()
fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 120, 0, 0))
before = termios.tcgetattr(slave)
child = subprocess.Popen([app], stdin=slave, stdout=slave, stderr=slave,
                         env={**os.environ, 'TERM': 'xterm-256color'})
children = []
resources = []
output = bytearray()
started = time.monotonic()
last_churn = last_sample = started
created = 0
try:
    while time.monotonic()-started < duration:
        now = time.monotonic()
        ready, _, _ = select.select([master], [], [], 0.05)
        if ready:
            output.extend(os.read(master, 65536))
            if len(output) > 1000000:
                del output[:-500000]
        assert child.poll() is None, 'monitor exited during churn'
        if now-last_churn >= 0.2:
            children.append(subprocess.Popen(['/bin/sleep', '0.1']))
            created += 1
            children = [p for p in children if p.poll() is None]
            last_churn = now
        if now-last_sample >= 1:
            status = Path(f'/proc/{child.pid}/status').read_text()
            rss = int(next(line for line in status.splitlines() if line.startswith('VmRSS:')).split()[1])
            fds = len(list(Path(f'/proc/{child.pid}/fd').iterdir()))
            if now-started > 5:
                resources.append((rss, fds))
            last_sample = now
            os.write(master, b'jkmric')
    assert resources and b'%' in output, 'no sustained monitoring samples'
    rss_values, fd_values = zip(*resources)
    # Heap arenas and varying live process populations need not return every page.
    assert max(rss_values)-min(rss_values) < 8192, 'RSS grew by more than 8 MiB'
    assert max(fd_values)-min(fd_values) <= 5, 'descriptor count accumulated'
    child.send_signal(signal.SIGTERM)
    child.wait(timeout=5)
    assert child.returncode == 0
    assert termios.tcgetattr(slave) == before
    print(f'Soak {duration:g}s: {created} short-lived processes; RSS {min(rss_values)}..{max(rss_values)} KiB; '
          f'FDs {min(fd_values)}..{max(fd_values)}; no crash; SIGTERM/terminal restoration passed')
finally:
    for process in children:
        process.wait(timeout=2)
    if child.poll() is None:
        child.kill()
        child.wait()
    os.close(master)
    os.close(slave)
