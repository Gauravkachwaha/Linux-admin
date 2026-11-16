# Linux Process Monitor — Product Requirements Document

## 1. Project overview

Linux Process Monitor is a lightweight, read-only terminal application written in C++20, inspired by `top` and `htop`. It presents live CPU, memory, and process information using Linux `/proc` data and an interactive ncurses interface.

The project is a focused Linux systems programming portfolio project: accurate metrics, robust parsing, and a usable terminal experience take priority over broad administration features.

## 2. Goals

- Make overall and per-core CPU usage, RAM, and swap usage easy to inspect.
- Help users identify resource-intensive processes and inspect their details.
- Demonstrate modern C++, Linux/POSIX APIs, resource ownership, and automated testing.
- Remain lightweight, understandable, and usable without root privileges.

## 3. Target users

- **Linux users:** Check resource usage and find active processes.
- **Developers:** Inspect application CPU usage, memory consumption, and thread counts.
- **Systems programming learners and reviewers:** Explore a well-scoped C++ project with reliable metrics and clear separation of responsibilities.

## 4. Functional requirements

### FR-01: CPU monitoring

- Display overall CPU utilization and utilization for each logical CPU core using successive `/proc/stat` samples.
- Express utilization as the percentage of elapsed CPU counter time spent non-idle; treat idle and I/O-wait as idle for this display and avoid double-counting guest time.
- Show a pending or unavailable value when no valid sample pair exists, including first refresh, invalid counter deltas, or newly detected cores.

### FR-02: RAM and swap monitoring

- Read RAM and swap statistics from `/proc/meminfo`.
- Show total, used, and available RAM, with used RAM defined as total minus available RAM.
- Show total, used, and free swap, with used swap defined as total minus free swap.
- Use clearly labeled, consistent binary units and handle systems with no swap without division errors.

### FR-03: Process enumeration and information

- Refresh the list of visible processes from numeric `/proc/[pid]` directories.
- Display PID, PPID, process name, state, user, CPU percentage, resident memory (RSS), and thread count.
- Resolve user IDs to names where possible, falling back to the numeric UID.
- Calculate process CPU percentage from changes in its user and system CPU time over the actual elapsed sampling interval. Define 100% as one fully utilized logical CPU; multithreaded processes may exceed 100%.
- Distinguish process instances using PID and start time so PID reuse does not produce incorrect CPU deltas or details.
- Mark a new process's CPU percentage pending until a valid sample pair is available.

### FR-04: Process details

- Allow selection of a process to inspect its listed fields, full available command line, and human-readable start time with a clear time zone.
- Support viewing long command lines without silently losing content.
- Distinguish an empty command line from inaccessible data. If the process exits, report that it is no longer available.

### FR-05: Sorting and filtering

- Support ascending and descending sorting by CPU percentage, RSS memory, and PID; use PID to break ties consistently.
- Support exact PID filtering and case-insensitive name substring filtering, with a clear way to choose and clear the filter.
- Preserve the chosen sort and filter during refreshes and display a clear empty state when nothing matches.

### FR-06: Refresh and terminal interface

- Refresh resource metrics and the process list every one second under normal operation. Use actual elapsed time for rate calculations rather than assuming every refresh occurs exactly on schedule.
- Provide an ncurses resource summary, scrollable process table, and selected-process details.
- Support keyboard navigation, sorting, filtering, help, and quitting while monitoring continues.
- Preserve selection across refreshes while the same process instance exists.
- Handle terminal resizing and show a readable size warning when content cannot fit.
- Restore terminal settings on normal exit and orderly interruption.

## 5. Non-functional requirements

- **Technology:** Use C++20, Linux/POSIX APIs, `/proc`, ncurses, and CMake. Read monitoring data directly rather than parsing external commands.
- **Maintainability:** Use modern C++ and RAII for resource ownership. Keep data collection, parsing/metric calculations, and presentation modular without prescribing a complex architecture.
- **Correctness:** Document metric definitions, convert kernel units correctly, and distinguish unavailable data from zero usage.
- **Robustness:** Handle disappearing processes, permission errors, missing fields, and malformed input gracefully. An inaccessible process must not stop monitoring other processes; a missing essential system source must produce a clear error.
- **Responsiveness:** Keep keyboard interaction responsive between refreshes. Retain only the data needed for current views and successive samples; avoid unbounded history and resource growth.
- **Permissions and safety:** Run as an ordinary user, respect `/proc` access restrictions, and perform no administrative mutations. Render process names and command lines safely without interpreting embedded terminal control sequences.
- **Usability:** Label units and states clearly, provide discoverable key bindings, and avoid conveying information through color alone. Display information visible in the current Linux environment without claiming full host visibility.
- **Testing:** Provide automated tests for parsing and metric calculations that can run without ncurses interaction or root access.

## 6. MVP scope

The MVP includes all functional requirements above: overall and per-core CPU monitoring, RAM and swap statistics, live process enumeration, process details, sorting, filtering, one-second refresh, and an interactive ncurses UI.

Delivery includes a CMake-based C++20 build and automated parsing and calculation tests. The application remains read-only. Advanced administration, historical analytics, and additional monitoring domains are not prerequisites for completion.

## 7. Acceptance criteria

1. The project builds with CMake and a C++20-capable compiler on a documented Linux environment, and runs as a non-root user with ncurses.
2. Overall and per-core CPU values match expected results from known successive counter samples within display rounding. Tests cover first samples, zero or invalid deltas, and guest-time accounting.
3. RAM and swap values match known `/proc/meminfo` fixtures, including correct units, missing fields, and a system with no swap.
4. The process table displays every required field for readable processes. Automated tests cover process names containing spaces or parentheses, malformed records, and unit conversions.
5. Process CPU calculations pass known-sample tests, including multithreaded usage above 100%, new processes, and PID reuse. Process details display the available command line and correctly converted start time.
6. CPU, RSS, and PID sorting work in both directions. PID and name filters return matching processes and remain active across refreshes.
7. In a live session, resource values and the process list update approximately once per second; navigation, details, help, and filtering remain usable during refreshes.
8. Processes starting or exiting during collection and inaccessible process files do not crash the application or leave misleading details for a reused PID.
9. Resizing and quitting work cleanly, and normal exit or orderly interruption restores the terminal. A sustained monitoring session shows no continuing growth in retained samples or open resources.
10. Automated parsing and metric tests pass, and no MVP workflow requires root privileges or modifies processes or system configuration.

## 8. Out-of-scope features

- File management.
- Arbitrary command execution, including fork/exec workflows.
- IPC worker architecture.
- Network monitoring.
- Service management.
- User management.
- Process termination or other process-control actions in the MVP.
- Web interfaces.
- Kernel modules.
- Remote administration, persistent monitoring history, and alerting.

This PRD defines product behavior and constraints only; implementation modules and detailed architecture are future work.
