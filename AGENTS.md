# AGENTS.md

## Project Management Loop

For behavior-changing work in this repository:

1. Check alignment with `docs/project-vision.md` before implementation.
2. Update `docs/roadmap-current.md` when priorities or sequencing change.
3. Before claiming completion, run:
   - `cd build && cmake --build . -j$(nproc)` (local build)
   - `ctest --output-on-failure` (test suite)
4. Append a handoff entry to `docs/session-handoffs.md` including:
   - what changed
   - why
   - status
   - next 1-3 steps
   - verification commands/results

## Cross-Compile Verification

For changes that affect Pi deployment:
- `./cross-build.sh` (Docker cross-compile for aarch64)
- Deploy and test on Pi when hardware-dependent

## Scope Note

This local file defines repo-specific workflow expectations.
Platform-level safety and skill instructions still apply.
