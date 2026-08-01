# Session Handoffs

Newest entries first.

---

## 2026-08-01 — Fresh source install protocol-submodule recovery

**What changed:** corrected `install.sh` so a normal initialized protocol
submodule (whose `.git` is a file) is never mistaken for an absent checkout.
The installer now uses a dedicated helper to fetch only the OAA `dist` branch,
normalize its fetchspec, retain conventional gitfile ownership, and check out
the exact gitlink pinned by the Prodigy source tree. A real temporary-repository
regression covers both fresh initialization and an immediate initialized rerun.
The hands-off protocol content was not edited.

**Status:** VERIFIED LOCALLY AND ON `prodigy2.local`; full installer resumption
awaits the operator entering the local sudo password. The reviewed patch is at
`9e934faccc89b39f2d387bc4fc6e9a2d9da34fe3` and remains two local commits ahead
of `origin/dev`. The release checkout on `prodigy2` is intentionally patched
with `install.sh` plus the new helper; its superproject remains `cd4b6b0` and
its protocol checkout remains the v1.5 pin `5ff4aa2` with a `dist`-only
fetchspec.

**Verification:** the regression was observed failing against both the
original clone-over-gitfile path and the first default-branch submodule fix.
`python3 tests/test_install_source_lifecycle.py`,
`python3 tests/test_ops_install_contracts.py`,
`python3 tests/test_install_list_prebuilt.py`,
`python3 tests/test_installer_hardware_contracts.py`, `bash -n install.sh
scripts/initialize-protocol-submodule.sh`, and `git diff --check` passed. The
helper also completed directly on `prodigy2` and preserved both exact SHAs.

**Review:** Opus pass 1 reported BLOCKER=0, MAJOR=1, MINOR=2. The branch-scope,
pin-integrity, shallow-clone, and inert-test concerns were remediated. Pass 2
reported BLOCKER=0, MAJOR=1, MINOR=4. The orphaned-gitdir recovery edge, direct
SHA fallback, tracked-URL resync, and related migration coverage were confirmed
or retained as nonblocking follow-up; the full `dist` history is an intentional
pin-integrity tradeoff. Those leads are recorded in the engineering backlog.

**Next 1–3 steps:** rerun `bash install.sh --mode source` locally on
`prodigy2`; confirm the source build and service installation complete; obtain
explicit user authorization before pushing the two installer commits.
