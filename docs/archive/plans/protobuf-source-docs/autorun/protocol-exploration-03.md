# Phase 03: Deploy to Pi and Run Baseline Capture

> **Context:** Sync the ProtocolLogger code to the Pi, build natively, and run the first baseline capture to verify the logging pipeline works end-to-end.
>
> **Reference:** `docs/plans/2026-02-23-protocol-exploration-implementation.md` (Task 6)
>
> **Prerequisites:** Phases 01-02 complete. Pi accessible at 192.168.1.149. Phone connected via ADB on dev VM.
>
> **IMPORTANT:** This phase requires Pi hardware and an interactive capture session. The agent should do the deploy+build, but the actual capture requires user interaction (pressing Enter after interacting with AA).

- [x] **Rsync source to Pi and build natively.** *(Clean build on Pi — 100% success, ProtocolLogger compiled and linked. Codex reviewed code pre-deploy: thread safety OK, noted architectural layering issue with aasdk→app dependency and per-message flush latency — both acceptable for exploration.)* Run: `rsync -avz --exclude='build/' --exclude='.git/' --exclude='libs/aasdk/lib/' /home/matt/claude/personal/openautopro/openauto-prodigy/ matt@192.168.1.149:/home/matt/openauto-prodigy/` — CRITICAL: the `--exclude='libs/aasdk/lib/'` prevents x86 .so files from overwriting ARM64 builds on the Pi. Then build on Pi: `ssh matt@192.168.1.149 "cd /home/matt/openauto-prodigy/build && cmake .. && cmake --build . -j3"`. Verify clean build with ProtocolLogger included. Do NOT commit — this is a deploy step.

- [x] **Run baseline capture and collect logs.** *(50 protocol log lines captured — full handshake through channel opens, AV setup, sensor starts. 2867 AA-filtered phone logcat lines. v1.7 negotiation confirmed. Unknown CONTROL message 0x0017 observed. Phone requested 9 channels and sensors: DRIVING_STATUS(1), COMPASS(13), ACCELEROMETER(10). App segfaulted later in session but after protocol data was captured. Timeline merged with -55761s clock offset.)* This is an INTERACTIVE task. Run `./Testing/capture.sh baseline` from the dev VM. The script will: reconnect AA, wait for user to interact with AA and press Enter, then collect logs. After capture, verify `Testing/captures/baseline/pi-protocol.log` has protocol messages and `Testing/captures/baseline/phone-logcat.log` has AA-filtered entries. Run the merge: `python3 Testing/merge-logs.py Testing/captures/baseline/pi-protocol.log Testing/captures/baseline/phone-logcat.log Testing/captures/baseline/timeline.md`. Commit capture data: `data: baseline protocol capture`
