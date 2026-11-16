"""Optional stdlib-only PTY checks; the application itself never launches commands."""
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


def exercise(command, exit_signal=None, fixture=False, error=False):
    owned = subprocess.Popen(['/bin/sleep', '30']) if not fixture and not error else None
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 110, 0, 0))
    before = termios.tcgetattr(slave)
    child = subprocess.Popen(command, stdin=slave, stdout=slave, stderr=slave,
                             env={**os.environ, 'TERM': 'xterm-256color'})
    output = bytearray()

    def drain(seconds=0.15):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            ready, _, _ = select.select([master], [], [], max(0, end-time.monotonic()))
            if ready:
                output.extend(os.read(master, 65536))

    def send(keys):
        os.write(master, keys)
        drain()

    try:
        drain(0.5)
        assert b'Linux Process Monitor' in output, output[-2000:]
        assert b'Overall CPU' in output and b'RAM:' in output and b'Swap:' in output
        if not fixture and not error:
            # Leave the filter editor active while multiple sampling deadlines pass.
            start = len(output)
            send(b'/')
            drain(2.2)
            assert b'%' in output[start:], 'CPU percentages did not refresh during editing'
            send(b'\n')
        if error:
            assert b'fixture access denied' in output and b'No matching processes' in output
        else:
            send(b'jkmric[]')
            send(b'h')
            assert b'Keys' in output and b'100%' in output
            send(b'h')
            send(b'/no_matching_process_987654\n')
            assert b'No matching processes' in output
            send(b'x')
            send(b'p100\n' if fixture else f'p{owned.pid}\n'.encode())
            send(b'jd')
            if fixture:
                assert b'Command line' in output and b'unsafe' in output
                assert b'unsafe\x1b[31m' not in output, 'untrusted escape reached terminal'
                send(b'\x1b[6~\x1b[6~\x1b[6~')
                assert b'END_OF_LONG_ARGUMENT' in output, 'long argument inaccessible'
            if owned is not None:
                assert b'Command line' in output and b'Start:' in output
                owned.terminate()
                owned.wait(timeout=3)
                drain(1.3)
                assert b'no longer available' in output, 'exited process details remained visible'
            send(b'dx')
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 10, 40, 0, 0))
        child.send_signal(signal.SIGWINCH)
        drain()
        assert b'Terminal too small' in output
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 30, 110, 0, 0))
        child.send_signal(signal.SIGWINCH)
        drain()
        if exit_signal:
            child.send_signal(exit_signal)
        else:
            send(b'q')
        child.wait(timeout=4)
        drain()
        assert child.returncode == 0, child.returncode
        assert termios.tcgetattr(slave) == before, 'terminal attributes not restored'
    finally:
        if owned is not None and owned.poll() is None:
            owned.terminate()
            owned.wait(timeout=3)
        if child.poll() is None:
            child.kill()
            child.wait()
        os.close(master)
        os.close(slave)


if __name__ == '__main__':
    app, fixture = sys.argv[1:]
    exercise([fixture], fixture=True)
    exercise([fixture, 'error'], error=True)
    exercise([app])
    exercise([app], signal.SIGINT)
    exercise([app], signal.SIGTERM)
    print('PTY: navigation, filters, details/long arguments, help, resize, quit/signals and termios restoration passed')
