# Linux Process Monitor

A lightweight, read-only Linux process monitor written in C++20. It reads `/proc` directly and presents live resource and process information in an ncurses terminal interface. No root privileges, background workers, shell commands, or network access are needed by the application.

## Features

- Overall and per-logical-core CPU utilization, refreshed approximately once per second.
- RAM total/used/available and swap total/used/free, in binary units.
- Process PID, PPID, name, state, effective user, CPU%, RSS, and thread count.
- Selected-process command line and local start time with a time-zone label.
- CPU/RSS/PID sorting in either direction and exact-PID or name-substring filtering.
- Identity-preserving selection, long-command-line scrolling, resize handling, and keyboard help.
- Explicit pending/unavailable states and graceful handling of process exits and restricted data.

## Build and run

Requirements: Linux with mounted `/proc`, a C++20 compiler, CMake 3.20 or newer, ncurses development headers/libraries, and terminal information matching `TERM`. Python 3 is optional for PTY integration checks; it is not an application dependency.

On Debian/Ubuntu the relevant development packages are `g++`, `cmake`, and `libncurses-dev`. Obtain them through your normal development-environment setup; run the monitor and tests as an ordinary user.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/linux-process-monitor
```

Use an interactive terminal of at least **80 columns × 24 rows**, locally or inside an existing SSH session. The program reports a clear error when stdin/stdout are not terminals. It has no command-execution or process-control functions.

For an optimized build:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
./build-release/linux-process-monitor
```

The validation environment was Ubuntu 26.04, kernel 7.0.0-43-generic, GCC 15.2, CMake 4.2.3, and ncurses 6.6. Other C++20-capable Linux toolchains are expected to work but were not verified here.

This workspace initially lacked ncurses development headers. For verification, the matching `libncurses-dev` package was downloaded and extracted without installation into `/tmp/linux-monitor-deps/prefix`. Existing build caches use these overrides:

```sh
cmake -S . -B build \
  -DCURSES_INCLUDE_PATH=/tmp/linux-monitor-deps/prefix/usr/include \
  -DCURSES_LIBRARY=/usr/lib/x86_64-linux-gnu/libncurses.so.6 \
  -DCURSES_EXTRA_LIBRARY=/usr/lib/x86_64-linux-gnu/libtinfo.so.6
```

These paths are environment-specific and temporary. A normal installation of the development package needs no overrides; a fresh machine must supply its own headers/libraries. No dependencies or generated build files are committed.

## Keyboard controls

| Key | Action |
| --- | --- |
| Up/Down or `k`/`j` | Select a process; scroll when viewing details |
| Page Up/Page Down | Move through the table or details |
| `c`, `m`, `i` | Sort by CPU%, RSS, or PID |
| `r` | Reverse sort direction |
| `/` | Edit a case-insensitive name-substring filter |
| `p` | Edit an exact numeric PID filter |
| Enter/Esc while editing | Finish editing; keep the current query |
| Backspace while editing | Remove a query character |
| `x` | Clear the filter |
| `d` or Enter | Toggle selected-process details |
| Esc | Close details/help |
| `[` / `]` | Previous/next logical CPU page |
| `h` or `?` | Toggle help |
| `q` | Quit outside the filter editor |

Input and monitoring continue during filter editing. A hidden or exited selection is cleared; use navigation to explicitly select another process. Unavailable sort values always appear last, and equal values use ascending PID as a tie-breaker. Details preserve argument boundaries using quotes and allow scrolling through the entire available command line. Non-ASCII/control bytes are escaped rather than interpreted by the terminal.

## Metric definitions

- **System/core CPU:** Counter differences from `/proc/stat`. Total includes user, nice, system, idle, I/O-wait, IRQ, softIRQ, and steal. Usage is `100 × (delta(total) - delta(idle + iowait)) / delta(total)`. Guest fields are already included in user/nice and are not added again.
- **Process CPU:** `100 × delta(utime + stime) / (CLK_TCK × actual elapsed seconds)`. Each process is timestamped near its read using a monotonic clock. One fully utilized logical CPU is 100%; multithreaded processes may exceed 100%. Child-process times are excluded.
- **Memory:** `/proc/meminfo` kB values are converted with 1024 bytes per unit. RAM used is total minus available; swap used is total minus free. Zero swap is valid.
- **RSS:** `/proc/[pid]/stat` resident pages multiplied by `sysconf(_SC_PAGESIZE)`. This is resident memory, not unique/private memory.
- **Identity:** PID plus start ticks. CPU baselines, selection, and details never intentionally carry across reused identities.
- **Start time:** `/proc/stat` boot epoch plus start ticks divided by `sysconf(_SC_CLK_TCK)`, displayed in the local time zone. If boot time is unavailable, realtime minus `/proc/uptime` provides an explicitly approximate fallback.
- **User:** Effective UID from `/proc/[pid]/status`, resolved through `getpwuid_r`; unresolved IDs are shown numerically.

First/new/recovered CPU samples are **pending**. Invalid deltas, missing sources, and inaccessible fields are **n/a/unavailable**, never fabricated zeroes. Counter regressions reset baselines. Detected suspend/realtime discontinuities exceeding two seconds also reset rate baselines. Slow scans skip missed deadlines rather than performing catch-up bursts.

## Architecture

`SystemMonitor` and `ProcessMonitor` own collection and previous samples. Pure parsers, metric calculations, and view helpers form `monitor_core`, which does not link ncurses. `TerminalUI` owns the terminal session and view state; the small application loop owns current snapshots and coordinates sampling/input. RAII closes descriptors and restores the terminal on normal exit or handled SIGINT/SIGTERM.

See [ARCHITECTURE.md](ARCHITECTURE.md), [PRD.md](PRD.md), [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md), and [TASKS.md](TASKS.md). Small presentation and scheduling helpers are separate so their behavior can be tested without a terminal.

## Verification

CTest contains:

- `core`: deterministic parsing, arithmetic, identity, recovery, selection, formatting, and scheduling checks. Temporary fixture directories exercise missing/denied process files without touching system configuration.
- `live`: the core checks plus direct Linux sampling, source comparisons, self-process/user discovery, and a controlled CPU workload.
- `terminal` (when Python 3 is available): stdlib-only pseudo-terminal checks against fixture and real snapshots, covering keys, filters, sorting, help, full long-argument scrolling, safe rendering, resize, live refresh during editing, process disappearance, and terminal restoration after quit/SIGINT/SIGTERM.

Run the deterministic suite alone with `ctest --test-dir build -R '^core$' --output-on-failure`. Tests intentionally exercise permission denial and should run as an ordinary user, not root.

For a longer resource/churn check:

```sh
python3 tests/soak_test.py build/linux-process-monitor 60
```

Only test harnesses launch disposable child workloads. The monitor itself never launches or signals another process. The soak check measures RSS/descriptors after warm-up while processes start and exit; its bounded-growth thresholds accommodate allocator behavior and changing system populations. It is evidence for the tested duration, not a guarantee of indefinitely constant memory use.

Final validation: 117 deterministic checks, 126 checks including live sampling, and the PTY suite passed. A 60-second churn run created 289 short-lived processes with RSS steady at 5,832 KiB and 3–6 transient open descriptors. Final validation results and acceptance-criterion coverage are recorded in `TASKS.md`. Debug/default, Release, and AddressSanitizer/UndefinedBehaviorSanitizer builds are exercised during project verification; sanitizer flags can be supplied through ordinary CMake compiler/linker flag variables without adding runtime dependencies to normal builds.

## Limitations

- `/proc` snapshots are not atomic. A process may exit or change fields during collection; directory-relative reads and identity revalidation prevent stale PID associations but do not freeze process state.
- Restricted `/proc` mounts can hide processes entirely. Values reflect the current namespace/kernel view, not container CPU/memory quotas.
- Sampling and user lookup are synchronous. Slow filesystem or NSS operations can delay refresh/input; the program makes no hard real-time guarantee.
- Kernel process names may already be truncated; the available full command line is shown separately. Command-line data may be empty, restricted, or changed by the process itself.
- UID mappings are cached while that UID remains present. Local account-name changes may require restarting the monitor to appear.
- All logical cores are accessible through CPU pages; very large process counts and exceptionally large command lines increase memory and rendering work proportionally.
- Terminal cleanup cannot be guaranteed after SIGKILL, power loss, or fatal system failure.

File management, arbitrary command execution, IPC workers, network/service/user management, process termination, web interfaces, and kernel modules remain out of scope.
