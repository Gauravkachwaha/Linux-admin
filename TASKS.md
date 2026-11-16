# Linux Process Monitor — Implementation Tracker

`IMPLEMENTATION_PLAN.md` is the roadmap; this file records verified implementation progress. Scope and engineering decisions remain governed by `PRD.md`, `AGENTS.md`, and `ARCHITECTURE.md`.

**Execution authorization:** The user explicitly authorized continuous implementation of all milestones, verification, and local commits without intermediate approvals. This overrides the pause rules below for this execution.

## Tracking rules

Execute milestones in order, 1–11, with explicit user instruction before starting each one. Technical dependencies below supplement this execution order; they do not authorize skipping ahead. Keep changes read-only, single-process, single-threaded, and within the approved scope.

Status values: **Not Started** (no implementation), **In Progress** (authorized work underway), **Blocked** (record the obstacle and remaining checks), **Completed** (all tasks and Definition of Done verified). Only check a task after its work and relevant verification are complete. Update status, checkboxes, and evidence after each milestone; record blockers as soon as discovered. Reopen completed work only with a documented reason, preserving prior evidence.

Every milestone's final verification task includes configuring/building and running CTest using the commands in `AGENTS.md`, resolving new warnings/failures, and reporting changed files, actual results, unrun checks, and known issues. Never mark a milestone Completed while required verification is incomplete. Stop after reporting; wait for instruction before proceeding.

**Overall progress:** 3 of 11 milestones completed. See milestone evidence below.

## Milestone 1 — Project Foundation

**Status:** Completed  
**Dependencies:** None; approved documents and Linux/toolchain prerequisites.  
**Evidence / blockers:** GCC 15.2, CMake 4.2.3, Ubuntu 26.04; ncurses 6.6 headers extracted under /tmp without root. Configure/build and CTest passed; intentional runner failure was detected, removed, and retested. Non-root executable smoke passed. No blockers.

- [x] Verify and record the Linux, C++20 compiler, CMake/CTest, and ncurses development environment.
- [x] Create the minimal project structure and application entry point with successful launch/exit only.
- [x] Configure target-scoped C++20, disable extensions, and enable all strict warnings specified in `AGENTS.md`.
- [x] Establish the reusable core target and include paths; use an interface target until compiled logic exists, without dummy functions.
- [x] Register a release-safe explicit-failure test runner with CTest.
- [x] Verify CTest discovery and failure reporting using a temporary intentional failure, then remove it.
- [x] Configure/build, run CTest and non-root executable smoke checks, and record the milestone report.

**Definition of Done:** Warning-clean build, successful ordinary-user launch/exit, and working CTest runner; no monitoring or UI implementation.

## Milestone 2 — CPU Data Parsing and Calculations

**Status:** Completed  
**Dependencies:** Milestone 1.  
**Evidence / blockers:** Pure CPU fixtures cover guest accounting, core IDs/changes, first/invalid samples, malformed fields and overflow. Warning-clean build, CTest and executable smoke passed. No blockers.

- [x] Add CPU counter and valid/pending/unavailable value types; convert the core target to a compiled library.
- [x] Parse aggregate/per-core `/proc/stat` records, numeric core IDs, and optional boot time using pure functions.
- [x] Calculate utilization from successive counters with idle plus I/O-wait treated as idle and no double-counted guest time.
- [x] Validate fields, ranges, sums, counter regressions, zero deltas, and impossible idle deltas.
- [x] Test known percentages, first samples, guest accounting, and reordered/added/removed cores using deterministic fixtures.
- [x] Test missing/malformed fields, invalid deltas, and overflow without live sources or clocks.
- [x] Build, run all tests and the existing executable smoke check, and record the milestone report.

**Definition of Done:** Pure CPU parsing/calculations match independent expected results, distinguish pending/unavailable from zero, and handle core identities correctly. Live baseline ownership remains deferred.

## Milestone 3 — Memory Monitoring

**Status:** Completed  
**Dependencies:** Milestones 1–2.  
**Evidence / blockers:** RAM/swap fixture calculations, independent validity, zero swap, units, malformed/duplicate/missing fields and overflow passed. Configure/build and full CTest passed without warnings. No blockers.

- [x] Parse RAM total/available and swap total/free fields with unit validation.
- [x] Convert kB to bytes with overflow checks and calculate used values with checked subtraction.
- [x] Keep RAM/swap validity independent and represent zero-swap systems correctly.
- [x] Test known values, no swap, missing fields, incorrect units, malformed numbers, overflow, and inconsistent totals.
- [x] Verify invalid RAM does not invalidate valid swap and vice versa.
- [x] Build, run all tests including CPU regressions, and record the milestone report.

**Definition of Done:** Deterministic memory tests pass with consistent units, independent error states, and no fabricated zero values or division errors.

## Milestone 4 — SystemMonitor Integration

**Status:** Not Started  
**Dependencies:** Milestones 2–3.  
**Evidence / blockers:** Not yet assessed.

- [ ] Implement scoped read-only system-source collection, once per source per sample.
- [ ] Publish structured CPU/RAM/swap snapshots with timestamps, boot time, and source errors.
- [ ] Maintain aggregate/core baselines by identity and prune disappeared cores.
- [ ] Clear failed baselines and establish fresh pending samples on recovery.
- [ ] Test success/failure/recovery, independent source errors, core changes, and pruning using supplied values/results.
- [ ] Check repeated live readings as a non-root user against direct source data, allowing for sampling-time differences.
- [ ] Build, run all tests, and record live-check results and the milestone report.

**Definition of Done:** Live snapshots and deterministic recovery behavior work; failures remain contextual and isolated. No recurring application loop or terminal presentation is added.

## Milestone 5 — Process Parsing and Identity

**Status:** Not Started  
**Dependencies:** Milestones 1–4.  
**Evidence / blockers:** Not yet assessed.

- [ ] Enumerate numeric `/proc` directories and establish ProcessMonitor's scoped collection foundation.
- [ ] Parse complete process `stat` records, preserving names with spaces/parentheses, and effective UID from `status`.
- [ ] Extract PID, PPID, name, state, CPU ticks, RSS pages, threads, and start ticks; define PID/start-tick identity.
- [ ] Validate `sysconf` tick/page values and implement checked RSS conversion.
- [ ] Use directory-relative reads and revalidate identity; skip vanished/identity-invalid records and label optional inaccessible data.
- [ ] Test complex names, truncated/malformed records, trailing fields, missing UID, PID mismatch/reuse, negative RSS, conversion overflow, and invalid/non-default constants.
- [ ] Verify non-root enumeration and graceful disappearance during collection.
- [ ] Build, run all tests, and record the milestone report.

**Definition of Done:** Readable records provide required raw fields and trustworthy identities; one inaccessible or vanished process does not abort enumeration. CPU history and details remain deferred.

## Milestone 6 — Process CPU Usage and Monitoring

**Status:** Not Started  
**Dependencies:** Milestone 5; sampling conventions from Milestones 2 and 4.  
**Evidence / blockers:** Not yet assessed.

- [ ] Calculate process CPU% from successive ticks and actual per-process monotonic timestamps adjacent to reads.
- [ ] Track baselines by identity, handling new processes, regressions, failed reads, and recovery.
- [ ] Resolve effective UIDs with numeric fallback and prune cached mappings to current users.
- [ ] Publish complete process snapshots, distinguish enumeration failure from an empty list, and prune obsolete baselines.
- [ ] Test known intervals, CPU above 100%, invalid elapsed time, overflow, regressions, first samples, PID reuse, and recovery.
- [ ] Test pruning, missing user data, and numeric UID fallback.
- [ ] Verify repeated live snapshots as a non-root user and note collection/lookup latency.
- [ ] Build, run all tests, and record the milestone report.

**Definition of Done:** All process columns exist as structured data; CPU history never crosses identities, retained state reflects current data, and the core remains ncurses-independent.

## Milestone 7 — Process Details

**Status:** Not Started  
**Dependencies:** Milestones 4–6.  
**Evidence / blockers:** Not yet assessed.

- [ ] Read complete selected-process `cmdline` contents with directory-relative access and identity revalidation.
- [ ] Parse NUL-separated arguments while preserving boundaries and valid empty content.
- [ ] Calculate start epoch from boot time/start ticks and provide time-zone-labeled formatting.
- [ ] Implement and label the approximate uptime fallback when boot time is unavailable.
- [ ] Distinguish gone, inaccessible, malformed, and valid-empty results; retain only currently requested details.
- [ ] Test empty/multiple/long arguments, control bytes, inaccessible/disappeared processes, and PID reuse during collection.
- [ ] Test non-default tick rates, overflow, fixed start-time calculations, and uptime fallback with fixed inputs.
- [ ] Build, run all tests, and record the milestone report.

**Definition of Done:** Identity-safe details are prepared and tested without a terminal. No stale details transfer to reused PIDs; actual rendering/scrolling remains in Milestone 9.

## Milestone 8 — Sorting, Filtering, and Selection

**Status:** Not Started  
**Dependencies:** Milestone 6; Milestone 7 for details invalidation.  
**Evidence / blockers:** Not yet assessed.

- [ ] Extract small pure view helpers independent of ncurses, as allowed by the roadmap.
- [ ] Implement CPU/RSS/PID sorting in both directions, ascending PID tie-breaks, and unavailable values last.
- [ ] Implement exact-PID and case-insensitive name-substring filter modes and clearing.
- [ ] Build view indices without retaining pointers into replaced snapshots.
- [ ] Reconcile selection by identity, clearing hidden/gone selections and associated details while retaining sort/filter settings.
- [ ] Test directions, ties, unavailable values, empty results, invalid PID input, case matching, and filter clearing.
- [ ] Test reordered snapshots, filtering out selection, disappearance, and PID reuse.
- [ ] Build, run all tests without a terminal dependency, and record the milestone report.

**Definition of Done:** Deterministic view tests pass; settings persist and selection never silently moves to another process instance.

## Milestone 9 — ncurses Terminal Interface

**Status:** Not Started  
**Dependencies:** Milestones 4 and 6–8.  
**Evidence / blockers:** Not yet assessed.

- [ ] Implement RAII ncurses/window ownership and clear initialization errors; restrict ncurses linkage to presentation/application targets.
- [ ] Render aggregate/per-core CPU, RAM/swap, and every required process column with units and explicit data states.
- [ ] Add navigation, scrolling, sorting, filter editing, and discoverable help; record exact key bindings/layout.
- [ ] Add details and long-command-line scrolling with safe control-byte rendering and preserved argument boundaries.
- [ ] Handle empty/error states and terminal resizing, including insufficient space, without relying only on color.
- [ ] Restore terminal state on quit and orderly SIGINT/SIGTERM using flag-only signal handlers.
- [ ] Connect an initial live snapshot and input loop; use fixture snapshots for edge states without continuous refresh yet.
- [ ] Test pure formatting/safe-text/view behavior automatically where applicable.
- [ ] Manually verify all interactions, long details, resize, errors, quit, and interruption; record results.
- [ ] Build, run all tests, and record the milestone report.

**Definition of Done:** Required UI behavior works on supplied snapshots and cleanup is verified. The executable is an explicitly non-refreshing checkpoint, not the finished MVP.

## Milestone 10 — Refresh Loop and Integration

**Status:** Not Started  
**Dependencies:** Milestones 4 and 6–9.  
**Evidence / blockers:** Not yet assessed.

- [ ] Connect monitors/UI with one-second monotonic deadlines and input waits capped at 100 ms.
- [ ] Skip missed deadlines, keep filtering nonblocking, and avoid source reads for ordinary navigation.
- [ ] Replace snapshots and reconcile selection/settings through refresh and resize.
- [ ] Fetch only open selected details per sampling cycle or selection change and discard obsolete details.
- [ ] Check shutdown between reads; bound baselines, caches, and view retention to current data.
- [ ] Settle timing-discontinuity detection and reset affected baselines when detected.
- [ ] Test deadline advancement, delayed cycles, selection/replacement transitions, and resets with explicit time inputs.
- [ ] Manually verify cadence, input responsiveness, process churn, details invalidation, resize, and shutdown.
- [ ] Inspect repeated-scan state/file-descriptor retention and record synchronous I/O/UID lookup limitations.
- [ ] Build, run all tests, and record the milestone report.

**Definition of Done:** MVP workflows operate together without root, normal input stays responsive, retained state does not accumulate, and individual failures do not stop unaffected monitoring.

## Milestone 11 — Final Verification and Documentation

**Status:** Not Started  
**Dependencies:** Milestones 1–10.  
**Evidence / blockers:** Not yet assessed.

- [ ] Run a clean warning-free build, full CTest suite including release mode, and non-root launch checks.
- [ ] Review RAII ownership, descriptor cleanup, caches/baselines, and arithmetic guards.
- [ ] Map every PRD acceptance criterion to recorded automated/manual evidence; identify any unmet criterion.
- [ ] Verify disappearance and PID reuse deterministically; exercise denied reads and unavailable-data presentation.
- [ ] Perform sustained live monitoring with process churn and inspect memory/resource trends.
- [ ] Manually verify required fields/workflows, long details, resizing, quit, SIGINT, and SIGTERM.
- [ ] Write README build/test/run instructions, dependencies/toolchain, keys, metric definitions, architecture links, and limitations.
- [ ] Verify documented commands and consistency with actual read-only behavior.
- [ ] Resolve identified in-scope defects and rerun affected checks; record unresolved blockers honestly.
- [ ] Update tracker evidence/status and report final verification without starting additional work.

**Definition of Done:** All PRD acceptance criteria pass with recorded evidence, verified README instructions, and no unresolved completion blockers or out-of-scope behavior. Any incomplete required check keeps this milestone open.
