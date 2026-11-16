# Linux Process Monitor — Implementation Plan

## Execution rules

This plan implements `PRD.md` using `ARCHITECTURE.md` and the engineering rules in `AGENTS.md`. It authorizes no implementation by itself. Start each milestone only on explicit user instruction; complete its verification and report before stopping for the next instruction.

Each milestone must leave the existing application buildable and runnable, even before monitoring and presentation are integrated. Implement only that milestone's behavior. Keep the application single-process, single-threaded, read-only, and usable without root. No external monitoring commands, IPC, administration features, or speculative frameworks.

For every implementation milestone:

- Configure and build with `cmake -S . -B build` and `cmake --build build`.
- Run `ctest --test-dir build --output-on-failure`; the small suite should remain inexpensive enough to run in full.
- Resolve new compiler warnings and test failures. Report blocked or unperformed verification explicitly.
- Report changed files, verification results, and known issues. Do not begin the next milestone automatically.

Paths below are expected changes, not instructions to create empty files ahead of time. Add only the necessary types and functions as their behavior arrives. All automated tests remain non-root and terminal-independent; live/manual checks supplement deterministic tests.

## Milestone 1 — Project Foundation

**Objective:** Establish a minimal, working C++20 build and verification workflow.

**Prerequisites:** Approved documents; Linux, a C++20 compiler, CMake/CTest, and available ncurses development dependencies. Record the toolchain used.

**Dependencies:** None.

**Tasks:**

- Create the minimal entry point and reusable core target, with target-scoped C++20 settings and extensions disabled.
- Enable `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` for project targets on GCC/Clang.
- Establish include paths, CTest registration, and an explicit-failure test runner that works in release builds.
- Initially use an interface core target if it has no compiled implementation; convert it to a compiled library in Milestone 2. Avoid dummy production functions solely to satisfy the build system.
- Keep the executable minimal: successful launch and exit, with no monitoring or ncurses behavior yet.

**Expected files:** `CMakeLists.txt`, `src/main.cpp`, `tests/tests.cpp`. Introduce `include/monitor/` when its first real header is needed.

**Required tests:** Application launch/exit smoke check; CTest discovery and test-runner success. Verify a deliberately failing test-runner check fails during development, then remove the intentional failure.

**Definition of Done:** Clean configuration/build with no new warnings; executable exits successfully as an ordinary user; CTest runs successfully. No future milestone logic is present.

## Milestone 2 — CPU Data Parsing and Calculations

**Objective:** Establish deterministic aggregate and per-core CPU metrics without live collection.

**Prerequisites:** Working core target and test runner.

**Dependencies:** Milestone 1.

**Tasks:**

- Add CPU counter/value-state types and pure `/proc/stat` parsing, including core IDs and optional `btime`.
- Implement the approved counter-difference formula, treating idle plus I/O-wait as idle and excluding separately reported guest counters from totals.
- Validate required counters, numeric ranges, overflow, zero deltas, and decreasing counters.
- Test matching sample pairs by core ID, including added/removed cores; defer live baseline ownership to SystemMonitor in Milestone 4.

**Expected files:** `include/monitor/{types,parsing,metrics}.hpp`, `src/{parsing,metrics}.cpp`, `tests/tests.cpp`, `CMakeLists.txt`; CPU fixtures under `tests/fixtures/` only where useful.

**Required tests:** Known percentages, aggregate/per-core records, guest accounting, first sample, reordered/added/removed core IDs, zero/decreasing counters, impossible idle deltas, malformed/missing fields, and arithmetic overflow.

**Definition of Done:** Independently specified expected values pass without `/proc`, ncurses, or wall-clock dependencies. Pending and unavailable values remain distinct from zero. The foundation executable still runs.

## Milestone 3 — Memory Monitoring

**Objective:** Produce correct RAM and swap values from supplied text.

**Prerequisites:** Shared parsing/calculation conventions and error states established.

**Dependencies:** Milestones 1–2.

**Tasks:**

- Parse `MemTotal`, `MemAvailable`, `SwapTotal`, and `SwapFree` with unit validation.
- Convert kB to bytes safely; calculate RAM used and swap used with checked subtraction.
- Keep RAM and swap validity independent and represent no-swap systems correctly.

**Expected files:** `include/monitor/{types,parsing,metrics}.hpp`, `src/{parsing,metrics}.cpp`, `tests/tests.cpp`, relevant fixtures.

**Required tests:** Known totals/differences, zero swap, missing keys, wrong units, malformed numbers, conversion overflow, and available/free values greater than totals. Verify one invalid metric group does not invalidate the other.

**Definition of Done:** Memory calculations pass deterministic tests with clearly defined units and no fabricated zero values or division errors. Existing CPU tests remain green.

## Milestone 4 — SystemMonitor Integration

**Objective:** Collect live system data and publish structured system snapshots.

**Prerequisites:** Tested CPU and memory parsing/calculations.

**Dependencies:** Milestones 2–3.

**Tasks:**

- Implement SystemMonitor with scoped read-only source access and one read per system source per sample.
- Maintain per-core/aggregate baselines, monotonic sample timestamps, current memory values, boot time, and source errors.
- Discard failed CPU baselines, prune disappeared cores, and establish fresh pending baselines on recovery.
- Keep state transitions testable through supplied parsed values/results; avoid filesystem or clock mock frameworks.

**Expected files:** `include/monitor/system_monitor.hpp`, `src/system_monitor.cpp`, shared types/calculation files as needed, `tests/tests.cpp`, `CMakeLists.txt`.

**Required tests:** Success/failure/recovery sequences, independent source failures, core changes, and baseline pruning. Perform a separate non-root live sampling check against directly read source data; do not require exact equality between samples taken at different times.

**Definition of Done:** Repeated live samples produce plausible CPU/RAM/swap snapshots, errors remain contextual, and deterministic recovery tests pass. No terminal presentation or recurring application loop is added yet.

## Milestone 5 — Process Parsing and Identity

**Objective:** Collect and parse process records safely, before CPU-rate tracking.

**Prerequisites:** Established value/error conventions and collection ownership practices.

**Dependencies:** Milestones 1–4, especially parsing and RAII collection conventions.

**Tasks:**

- Enumerate numeric `/proc` directories; introduce ProcessMonitor's collection foundation without complete monitoring behavior.
- Parse complete `stat` records using the name delimiters and documented suffix positions, plus effective UID from `status`.
- Extract PID, PPID, name, state, CPU ticks, RSS pages, threads, and start ticks; define identity as PID plus start ticks.
- Validate positive tick/page constants from `sysconf`; implement checked RSS conversion without hardcoded page sizes.
- Use scoped directory-relative reads and verify identity after collecting associated fields. Skip vanished/identity-invalid entries and preserve unavailable optional fields.

**Expected files:** `include/monitor/process_monitor.hpp`, `src/process_monitor.cpp`, shared types/parsing/metrics files, `tests/tests.cpp`, process fixtures, `CMakeLists.txt`.

**Required tests:** Names with spaces and multiple parentheses, truncated records, unexpected trailing fields, malformed numbers, missing UID, PID mismatch, same PID/different start ticks, negative RSS, non-default page sizes, overflow, and invalid system constants. Check non-root enumeration and disappearing entries separately.

**Definition of Done:** Valid readable records contain the required raw fields and reliable identities. Unreadable or disappearing processes do not abort enumeration. No process CPU history, user interface, or details feature is implemented ahead of schedule.

## Milestone 6 — Process CPU Usage and Monitoring

**Objective:** Produce complete live process snapshots with accurate CPU rates and users.

**Prerequisites:** Tested process records, identities, and conversion constants.

**Dependencies:** Milestone 5; shared sampling conventions from Milestones 2 and 4.

**Tasks:**

- Calculate process CPU% from successive user/system ticks and actual per-process monotonic elapsed time adjacent to reads.
- Track previous counters by identity; handle first samples, changed identities, failures, and counter regression without leaking old values.
- Resolve effective UIDs through `getpwuid_r`, falling back to numeric UIDs; prune the UID cache to current users.
- Complete ProcessMonitor snapshots, distinguish directory failure from an empty list, and prune disappeared baselines each scan.

**Expected files:** ProcessMonitor, shared types/metrics, and `tests/tests.cpp`; collection helpers only where genuinely reused.

**Required tests:** Known elapsed intervals, CPU above 100%, zero/invalid intervals, overflow, regressions, failed reads/recovery, PID reuse, baseline pruning, numeric UID fallback, and missing user data. Perform non-root live snapshot and repeated-sampling checks.

**Definition of Done:** Required process columns are available as structured data, CPU history never crosses identities, and retained state reflects only current processes/users. No ncurses dependency enters the core.

## Milestone 7 — Process Details

**Objective:** Provide identity-safe command-line and start-time details as display-ready data.

**Prerequisites:** Complete process identities and snapshots; available boot-time metadata.

**Dependencies:** Milestones 4–6.

**Tasks:**

- Read the selected process's complete available `cmdline` with directory-relative access and identity revalidation.
- Parse NUL-separated arguments while preserving boundaries and valid empty content.
- Calculate start epoch from boot time and start ticks; provide time-zone-labeled formatting for later presentation.
- Implement the architecture's approximate uptime fallback when boot time is unavailable, with explicit approximation status.
- Return distinct gone, inaccessible, malformed, and valid-empty outcomes. Retain only currently requested details.

**Expected files:** ProcessMonitor, shared types/parsing/metrics files, `tests/tests.cpp`, command-line fixtures as needed.

**Required tests:** Empty and multi-argument contents, long command lines, embedded control bytes, inaccessible/gone outcomes, PID reuse during details collection, non-default tick rates, start-time overflow, fixed boot-time calculations, and uptime fallback with fixed realtime inputs.

**Definition of Done:** Details are correct and testable without a terminal; reused PIDs cannot inherit stale details. Actual ncurses display and scrolling remain in Milestone 9.

## Milestone 8 — Sorting, Filtering, and Selection

**Objective:** Implement deterministic view logic independently of ncurses.

**Prerequisites:** Stable process row and identity types.

**Dependencies:** Milestone 6; Milestone 7 for details invalidation behavior.

**Tasks:**

- Implement ascending/descending CPU, RSS, and PID sorting; use ascending PID for ties and place unavailable values last in either direction.
- Implement explicit exact-PID and case-insensitive name-substring filter modes and filter clearing.
- Return row indices or equivalent non-owning view values without retaining pointers across snapshot replacement.
- Reconcile selection by identity; clear it when gone or filtered out, and invalidate associated details.

**Expected files:** A small `include/monitor/view_logic.hpp` and `src/view_logic.cpp`, `tests/tests.cpp`, and `CMakeLists.txt`. This justified extraction keeps the core/test target independent of ncurses; it adds no controller class or framework.

**Required tests:** Both directions for every key, ties, unavailable fields, empty lists, PID exactness and invalid input, case-insensitive matching, clearing filters, reordered snapshots, disappearance, filtering out selection, and PID reuse.

**Definition of Done:** Tests pass with plain input values and no terminal. Sort/filter settings survive snapshot replacement; selection never silently transfers to another process instance.

## Milestone 9 — ncurses Terminal Interface

**Objective:** Provide complete interactive presentation using supplied snapshots.

**Prerequisites:** Tested snapshot, details, and view-logic contracts; ncurses available.

**Dependencies:** Milestones 4 and 6–8.

**Tasks:**

- Implement a scoped TerminalUI session and windows, summary/table rendering, navigation, scrolling, sort/filter editing, help, and details.
- Display every required field, units, pending/unavailable states, source errors, empty results, and long command lines with scrolling.
- Escape control sequences and preserve argument boundaries; do not rely solely on color.
- Handle resize/insufficient space and initialization failure; restore the terminal on normal exit and orderly SIGINT/SIGTERM interruption using flag-only handlers.
- Connect the executable to an initial supplied live snapshot and input loop; use fixture snapshots for edge-state verification. Continuous resampling remains Milestone 10.

**Expected files:** `include/monitor/terminal_ui.hpp`, `src/terminal_ui.cpp`, `src/main.cpp`, `CMakeLists.txt`; view-logic/tests only for new pure presentation behavior.

**Required tests:** Automated safe-text/formatting and view-state checks where applicable. Manually exercise navigation, both filter modes, sorting, help, long details, empty/error states, resize including too-small terminals, quit, and orderly interruption.

**Definition of Done:** All required UI operations work on supplied snapshots, terminal restoration is verified, and the ncurses dependency is limited to presentation/application targets. The executable is usable as a non-refreshing inspection checkpoint, not represented as the completed MVP.

## Milestone 10 — Refresh Loop and Integration

**Objective:** Deliver the integrated, continuously refreshing monitor.

**Prerequisites:** Working collectors and tested terminal interaction/lifecycle.

**Dependencies:** Milestones 4, 6–9.

**Tasks:**

- Coordinate monitors and UI in the small application loop with one-second monotonic deadlines and input waits capped at 100 ms.
- Refresh approximately once per second; skip missed deadlines rather than running catch-up scans. Keep filtering nonblocking.
- Replace current snapshots, reconcile identity-based selection, and refresh only open selected details once per sampling cycle or selection change.
- Preserve settings through refresh/resize, check shutdown between reads, and avoid source reads for ordinary navigation.
- Bound baseline/cache/view retention to current data; reset rate baselines on detected timing discontinuities rather than fabricate rates.

**Expected files:** `src/main.cpp`, monitors and TerminalUI where integration requires changes, shared metric/view functions, `tests/tests.cpp`.

**Required tests:** Deterministic deadline advancement, delayed cycles, replacement/selection transitions, and baseline reset behavior using explicit time inputs. Manually verify live refresh cadence, input during filtering, details on exit/reuse, resizing, shutdown, and process churn. Inspect retained state and file descriptors over repeated scans.

**Definition of Done:** All MVP workflows operate together as a non-root user. Monitoring remains responsive under normal local conditions, state does not accumulate across refreshes, and source/process failures do not terminate unaffected monitoring. Record synchronous I/O/UID-lookup latency limitations without introducing concurrency.

## Milestone 11 — Final Verification and Documentation

**Objective:** Validate the complete MVP against the PRD and document reproducible usage.

**Prerequisites:** Integrated application and all previous milestone checks complete.

**Dependencies:** Milestones 1–10.

**Tasks:**

- Run a clean build and full tests, including release-mode test execution; review warnings, ownership, descriptor cleanup, cache pruning, and arithmetic guards.
- Map each PRD acceptance criterion to deterministic or manual evidence. Fix discovered in-scope defects and rerun affected checks.
- Perform a sustained monitoring session with process churn; inspect memory/resource trends, permission errors, resize, selection, and shutdown.
- Document dependencies/toolchain, build/test/run commands, keys, metric definitions, architecture links, read-only scope, visibility restrictions, and latency/approximation limitations.
- Ensure documentation matches implemented behavior; no cosmetic redesign or optional features.

**Expected files:** `README.md`, relevant existing source/tests only for verified defects, and `CMakeLists.txt` only if verification exposes a build issue. Keep approved requirements/guidelines/architecture unchanged unless explicitly instructed.

**Required tests:** Full CTest suite and clean launch; manual end-to-end inspection of all required fields/workflows, long command lines, unavailable data, resize, quit, SIGINT/SIGTERM, and sustained operation. Verify PID reuse deterministically rather than depending on the host to recycle a PID on demand.

**Definition of Done:** All PRD acceptance criteria have recorded passing evidence or explicitly reported blockers; no blocker is silently waived. README commands are verified, the application needs no root privileges, and no out-of-scope behavior has been introduced.

## Dependencies, risks, and architecture alignment

The execution order is 1 through 11, with explicit instruction at each boundary. The major technical dependency chains are:

- Foundation → CPU/memory pure logic → SystemMonitor.
- Process parsing/identity → process rates/snapshots → details and view logic.
- Both monitor snapshots plus view logic → TerminalUI → refresh integration → final verification.

Key risks and handling:

- **Changing `/proc` data:** Use identity revalidation, explicit unavailable states, and deterministic failure/reuse sequences; live snapshots cannot be atomic.
- **Parsing and arithmetic:** Validate numeric ranges, units, complex names, and counter discontinuities before presenting values.
- **Synchronous delays:** Local collection and UID resolution may delay input. Measure in Milestones 6 and 10; do not preemptively add threads or workers.
- **Environment-dependent checks:** Permission restrictions, PID churn, terminal behavior, and time zones vary. Keep calculations deterministic and report manual-check environment/limitations.
- **Resource lifetime:** Exercise repeated refresh, selected command-line replacement, resize, and shutdown; check both memory and descriptor retention.

No blocking scope contradictions were identified. Two sequencing clarifications keep the plan faithful to the architecture: Milestone 7 prepares details for display, while actual terminal rendering belongs to Milestone 9; Milestone 9 uses an initial snapshot, while continuous sampling belongs to Milestone 10. The small view-logic extraction in Milestone 8 is justified by the explicit requirement to test it without linking ncurses and follows the architecture's allowance for separating view helpers.

Supported toolchain versions are recorded in Milestone 1, exact keys/layout in Milestone 9, and timing-discontinuity detection policy in Milestone 10. These are implementation details to settle within their authorized milestones, not new features or reasons to start implementation early.
