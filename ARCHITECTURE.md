# Linux Process Monitor — Architecture

## 1. Overview

A single-process, single-threaded C++20 application reads Linux monitoring data without modifying processes or system configuration. `PRD.md` defines scope; `AGENTS.md` defines engineering rules.

Data flows in one direction:

```text
/proc + Linux/POSIX APIs
    → SystemMonitor / ProcessMonitor collection
    → pure parsers → pure metric calculations
    → current application snapshot
    → TerminalUI (ncurses)
```

A small application loop coordinates sampling and input. The UI returns user intent, such as selecting a process or quitting; it does not read `/proc`. Collection, parsing, calculations, and presentation remain separate functions and modules without introducing services, inheritance hierarchies, IPC, worker processes, or threads.

## 2. Modules and contracts

The interfaces below describe responsibilities and data contracts, not final C++ declarations.

### SystemMonitor

- **Responsibilities:** Read system sources once per refresh, parse aggregate/per-core counters and memory statistics, and calculate current utilization.
- **Public interface:** `sample()` returns a `SystemSnapshot` containing aggregate/per-core CPU metrics, RAM/swap metrics, boot time when available, sample time, and source errors.
- **Inputs/outputs:** Raw source text is passed to pure parsers. Parsed `CpuCounters` and memory fields feed pure calculations that produce percentages and byte counts.
- **Ownership/lifetime:** Application-owned for the session. Owns only the previous valid CPU baselines, keyed by aggregate/core ID; removes disappeared cores. Collection resources are scoped RAII objects.
- **Dependencies:** `/proc/stat`, `/proc/meminfo`, a monotonic clock, parsing/calculation functions, and shared value types. No ncurses dependency.
- **Errors:** A failed source affects its own metrics; unaffected metrics remain usable. Missing baseline means pending, invalid data means unavailable. After a failed CPU read, discard that baseline and establish a fresh one on recovery.

### ProcessMonitor

- **Responsibilities:** Enumerate numeric PID directories, collect process fields, resolve users, track process identities, and calculate process CPU usage. Fetch details only for the selected process.
- **Public interfaces:** `sample()` returns a `ProcessSnapshot` with rows and collection status; `read_details(identity, boot_time)` returns `ProcessDetails` or an explicit gone/unavailable result.
- **Inputs/outputs:** Inputs include validated system tick/page constants and, for details, `ProcessIdentity` and boot time. Outputs contain PID, PPID, name, state, UID/user, CPU%, RSS bytes, threads, and identity; details add command-line arguments and start time.
- **Ownership/lifetime:** Application-owned for the session. Owns prior CPU counters and timestamps keyed by identity and a UID cache limited to users in the current snapshot. Retains no historical process list. Open directory/file handles are scoped and closed automatically.
- **Dependencies:** `/proc/[pid]` sources, `sysconf`, POSIX UID lookup, monotonic clock, and pure parsing/calculation functions. No UI dependency.
- **Errors:** Disappearing processes are skipped; missing optional fields become unavailable. If identity cannot be read, do not publish a selectable row with guessed identity. Directory-wide failure is reported separately from an empty list. Never reuse a previous row as current data after failure.

### TerminalUI

- **Responsibilities:** Render system summary, process table, and details; interpret navigation, help, sort/filter, resize, and quit input. Escape control bytes in displayed process data.
- **Public interfaces:** `handle_input(key)` updates view state and returns an action; `render(snapshot, details)` displays borrowed data; `selected_identity()` identifies any details request.
- **Inputs/outputs:** Consumes immutable snapshots and explicit data states. Produces only UI actions, never administrative operations.
- **Ownership/lifetime:** Application-owned RAII ncurses session; owns windows, filter text, sort mode/direction, selection identity, and scroll offsets. Snapshots remain owned by the application; no retained pointers into replaced rows.
- **Dependencies:** ncurses and shared data types. Sorting/filtering use ordinary pure helpers, independent of terminal calls.
- **Errors:** Show unavailable values and source errors in context. Handle empty results and insufficient terminal size. Initialization failure exits with a clear diagnostic; normal unwinding restores the terminal.

### Application state and shared functions

`main` owns the monitors, UI, current system/process snapshots, selected details, sampling deadline, and shutdown flag. No separate application framework or controller class is necessary.

Shared types are plain values: `ProcessIdentity` (PID and start ticks), counters, snapshots, rows, details, and a small status/error representation. Values distinguish valid, pending, and unavailable; an empty valid command line is not an error. Use C++20 facilities such as `optional` or `variant` for result representation rather than requiring C++23 `expected` or a generic error framework.

Pure parsers consume text and return parsed values or contextual parse errors. Pure calculations consume parsed values, previous samples, elapsed durations, and conversion constants. Keep file reads and POSIX calls out of both. Small private RAII helpers are justified for owned descriptors; introduce shared utilities only when actually reused.

## 3. Sources and metric definitions

| Source | Use |
| --- | --- |
| `/proc/stat` | Aggregate `cpu`, individual `cpuN` counters, and `btime` for calendar start times. Match cores by numeric ID, not row position. |
| `/proc/meminfo` | `MemTotal`, `MemAvailable`, `SwapTotal`, and `SwapFree`; validate expected units and convert kB to bytes with a factor of 1024. |
| `/proc/[pid]/stat` | PID (1), name (2), state (3), PPID (4), user/system ticks (14/15), threads (20), start ticks (22), and RSS pages (24). |
| `/proc/[pid]/status` | Effective UID from the second value of `Uid:`; resolve to a username, with numeric UID fallback. |
| `/proc/[pid]/cmdline` | Read the complete available byte sequence for selected details; split NUL-separated arguments and preserve argument boundaries in presentation. Empty contents are valid. |
| `/proc/uptime` | Not needed on the normal path. If `btime` is unavailable, its first value can provide an approximate boot epoch as current realtime minus uptime; mark the derived start time approximate. |
| `sysconf(_SC_CLK_TCK)` / `sysconf(_SC_PAGESIZE)` | Read and validate positive values once at startup for tick and RSS conversions; never hardcode them. |
| `getpwuid_r()` | Resolve effective UID using a reusable, size-checked buffer; lookup errors or absent entries fall back to numeric UID. |

The process `stat` parser locates the name's opening delimiter and final closing parenthesis before parsing the remaining fields by documented positions. It must not split the whole record on whitespace: names can contain spaces and parentheses. Read the complete record and validate the numeric suffix and expected PID. Kernel fields may be added; tolerate irrelevant trailing fields. [Linux process stat reference](https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html).

### CPU sampling

For aggregate and each core independently:

- `total = user + nice + system + idle + iowait + irq + softirq + steal`.
- `idle_total = idle + iowait`.
- `CPU% = 100 × (delta(total) − delta(idle_total)) / delta(total)`.

Guest counters are already included in user/nice accounting and are not added again. This follows the PRD convention that I/O-wait is idle. Reject decreasing contributing counters, zero total deltas, impossible idle deltas, and arithmetic overflow; publish unavailable and reset the baseline. Newly observed cores are pending. I/O-wait can decrease on Linux, so this is a recoverable invalid sample rather than an application failure. [Linux system stat reference](https://man7.org/linux/man-pages/man5/proc_stat.5.html).

Process CPU% is `100 × delta(utime + stime) / (ticks_per_second × elapsed_seconds)`. Use a monotonic timestamp taken adjacent to each process counter read, rather than one assumed interval for the entire scan. Do not include waited-for children, add guest time again, or divide by core count. Thus two fully busy threads can approach 200%. Reject counter regressions, invalid elapsed time, and overflow. Failed reads remove the corresponding baseline; a later valid instance starts pending.

### Memory, start time, and identity

RAM used is total minus available; swap used is total minus free. Validate each operand and subtraction. Missing RAM fields do not invalidate valid swap fields. Zero total swap is a valid no-swap state. RSS bytes equal nonnegative RSS pages multiplied by the validated page size, with overflow checks.

Calendar start time is `btime + start_ticks / ticks_per_second`, formatted with an explicit local time zone. Start ticks remain the identity value; formatted wall time is never used for identity. The optional uptime fallback includes suspend time and is sensitive to realtime changes and read skew, so it is approximate. [Linux uptime reference](https://man7.org/linux/man-pages/man5/proc_uptime.5.html).

A PID plus start ticks identifies the observed process instance. Open its directory read-only and use relative reads for its files; verify identity again after collecting associated status/details. On disappearance or mismatch, discard those associated results. This prevents joining data from reused PIDs, but `/proc` does not provide an atomic process snapshot: fields can still change within one instance, including during exec.

## 4. Refresh and event loop

1. Validate system constants, initialize scoped terminal resources, and take the first snapshot. CPU rates are initially pending.
2. Use a monotonic clock for a one-second sampling deadline. Wait for ncurses input for at most the remaining deadline, capped at 100 ms so shutdown requests are observed promptly.
3. Process navigation and filter edits without blocking text-entry dialogs. Redraw when state changes; do not reread monitoring sources for ordinary keypresses.
4. At the deadline, sample system data and processes, replace current snapshots, then rebuild the filtered/sorted row-index view. Advance to the next future deadline; skip missed deadlines instead of performing catch-up scans.
5. Retain selection by identity when still visible. If it exits, clear selection and report gone in open details; never silently select a reused PID. If filtering hides it, clear selection. Sort unavailable values last in either direction and break numeric ties by ascending PID. Apply exact PID or case-insensitive name filtering according to the selected mode.
6. Read selected details when opened/changed and at most once each refresh while visible. Retain only the selected command line, allow scrolling through its full available content, and discard it on selection change. Do not cache every process command line.
7. Handle ncurses resize notifications in normal execution context, recompute layout, and retain current monitoring/view state while displaying a size warning if necessary.
8. Quit input or SIGINT/SIGTERM requests ends the loop. Signal handlers only set a `sig_atomic_t` flag; no ncurses, allocation, or cleanup inside handlers. RAII restores terminal state and closes resources.

Collection is synchronous and normally short. Check shutdown between process reads. Slow filesystem or name-service operations can delay a refresh and input: a single-threaded design cannot promise hard response deadlines. Cache UID resolutions for current users and avoid repeated details reads; measure real behavior before considering any architectural change. Do not add threads preemptively.

Storage is proportional to currently visible processes/cores plus one selected command line: current snapshots, one previous baseline per identity/core, current UID mappings, and view indices. Replace/prune these each sample; retain no accumulating history or per-refresh error log.

## 5. Testing architecture

Build a core library without ncurses and a small test executable registered with CTest. Use deterministic text fixtures and explicit timestamps/conversion constants; no root, live machine state, terminal, or test framework dependency is required. Tests must fail with useful diagnostics even in release builds.

| Area | Required checks |
| --- | --- |
| System CPU | Known deltas, idle/I/O-wait convention, guest exclusion, first sample, zero/decreasing counters, overflow, and core appearance/disappearance. |
| RAM/swap | Expected totals and differences, kB conversion, no swap, absent keys, wrong units, malformed numbers, and inconsistent totals. |
| Process parsing | Spaces and multiple parentheses in names, required field positions, truncated/malformed records, empty/NUL-separated command lines, and missing UID. |
| Process CPU/identity | Known elapsed intervals, values above 100%, zero elapsed time, counter regression, first sample, and same PID with changed start ticks. |
| Conversions/details | Non-default tick/page sizes, invalid constants, RSS overflow, start epoch calculation, and approximate uptime fallback with fixed inputs. |
| View logic | Sort direction/ties/unavailable values, exact PID and name filtering, and selection retention/invalidation. |

Keep multi-sample state transitions testable through ordinary value inputs, without virtual filesystem or clock frameworks. Later integration checks cover process churn, denied reads, and terminal cleanup. UI changes also require manual refresh/navigation/resize checks. Follow the build and test commands in `AGENTS.md` once implementation exists.

## 6. Proposed structure

This is a proposal only; create files when their milestone requires them.

```text
AGENTS.md
PRD.md
ARCHITECTURE.md
CMakeLists.txt
include/monitor/
    types.hpp
    parsing.hpp
    metrics.hpp
    system_monitor.hpp
    process_monitor.hpp
    terminal_ui.hpp
src/
    main.cpp
    parsing.cpp
    metrics.cpp
    system_monitor.cpp
    process_monitor.cpp
    terminal_ui.cpp
tests/
    tests.cpp
    fixtures/             # only fixtures that benefit from separate files
```

Parser functions for the small set of sources share one module; calculations share another. Collection stays in the two monitors. View helpers can remain alongside TerminalUI until their size justifies a separate file. No `docs/` directory is needed for three root-level documents.

CMake will define a reusable core target, an ncurses-linked application, and a CTest test executable. Require C++20 without extensions and apply the strict target-scoped warning flags specified in `AGENTS.md`. Depend only on the standard library, Linux/POSIX facilities, and ncurses.

## 7. Decisions, limitations, and implementation readiness

- Single-process synchronous sampling keeps ownership and shutdown simple; no current requirement needs concurrency or IPC.
- Pure parsers/calculations provide deterministic correctness tests without architectural mock layers.
- Effective UID defines the user column. CPU is normalized per logical CPU for processes, but aggregate/core utilization stays within its valid 0–100% range.
- Read-only access and explicit unavailable states respect existing permissions. Restricted `/proc` mounts may hide processes entirely; absence is not evidence of zero activity.
- Data reflects the current namespace and kernel counters, not container resource quotas. RSS is kernel-reported resident memory, not unique/private memory; command lines and process names may change or be restricted.
- Sampling is approximate, not an atomic system snapshot. Wall-clock start times are display estimates with source precision limits. Clock changes and suspend warrant fresh rate baselines when detected rather than fabricated rates.
- No additional features or requirements are introduced. There are no blocking architecture questions; the supported Linux/compiler/ncurses baseline and exact key bindings/layout should be recorded during the relevant implementation milestone. Verify name-service latency and normal scan cost on the chosen environment before considering a scope change.
