# Engineering Guidelines

These instructions apply throughout this repository.

## Scope and workflow

- Read `PRD.md` before making changes; it is the approved product scope and acceptance contract.
- Implement only the explicitly requested task or milestone. Do not add features, expand the architecture, or change the PRD without instruction.
- Complete and verify the current task, then stop. Do not proceed to another milestone without explicit user instruction.
- Keep changes focused and avoid unrelated refactoring or new dependencies.

## Technology and design

- Use modern C++20, Linux/POSIX APIs, ncurses, and CMake.
- Read monitoring data directly from `/proc`; do not invoke or parse external monitoring commands.
- Prefer RAII, value semantics, and clear ownership. Wrap owned file descriptors and terminal resources so cleanup happens reliably; avoid manual owning pointers.
- Separate data collection, parsing, metric calculations, and ncurses presentation. Parsers and calculations must be testable without live `/proc` access or a terminal.
- Keep modules small and interfaces explicit. Avoid speculative abstractions, frameworks, worker processes, and unnecessary dependencies.
- Use target-scoped CMake settings, require C++20, and disable compiler language extensions. Enable strict warnings for project targets on GCC/Clang: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`. Resolve new warnings rather than broadly suppressing them.

## Monitoring correctness and safety

- Keep the application read-only and usable without root. Never add privilege escalation, process-control actions, file management, or arbitrary command execution.
- Follow the PRD's CPU and memory definitions. Compute CPU rates from successive valid samples and actual elapsed time; do not assume exactly one second elapsed.
- Identify process instances by PID and start time. Never carry CPU history or selected details across PID reuse.
- Convert kernel ticks, pages, and memory units explicitly using system-provided values where applicable. Do not double-count guest CPU time or clamp process CPU usage to 100%.
- Distinguish pending, unavailable, empty, and zero values. Validate parsed fields and arithmetic, including missing data, invalid deltas, overflow, and zero denominators.
- Treat process exit during inspection, permission denial, and inaccessible files as expected conditions. Continue monitoring unaffected processes and report essential system-source failures clearly.
- Safely render untrusted process names and command lines without interpreting terminal control sequences.
- Keep retained samples bounded, preserve responsive input, and restore terminal state on normal exit and orderly interruption. Keep any signal handler async-signal-safe.

## Tests and verification

- Add or update automated tests whenever parser or calculation behavior changes. Use deterministic fixtures and expected results independent of the implementation.
- Cover malformed and missing fields, names with spaces or parentheses, unit conversions, first samples, invalid deltas, guest accounting, no swap, multithreaded CPU usage, and PID reuse as relevant.
- Keep tests runnable without root or ncurses interaction. Register automated tests with CTest.
- After implementation changes, configure/build with CMake and run relevant tests. Once the build exists, the standard commands are `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`.
- For UI or lifecycle changes, also check affected navigation, refresh, resize, and terminal-restoration behavior interactively when possible.
- Fix failures introduced by the task before declaring completion. If verification is blocked, report the exact limitation; never claim an unrun check passed. Documentation-only tasks do not require a build.

## Task completion report

After each task, report:

- Changed files and the purpose of the changes.
- Verification commands/checks and their results, including anything not run.
- Known issues or blockers, or explicitly state that none were identified.

Do not begin the next milestone as part of the completion report.
