# tests/

Unit and integration tests, built with CTest + Qt Test.

## Running

```bash
cd build && ctest --output-on-failure     # full suite
ctest -R <name> -V                         # single test, verbose
```

**ctest does NOT compile `main.cpp` — always build the app target too before claiming green:**

```bash
cmake --build . --target openauto-prodigy -j$(nproc)
```

A broken `main.cpp` is invisible to the test suite (a cached object file masked exactly this once).

## Layout & conventions

- One file per subject: `tests/test_<subject>.cpp` (~100 files covering config, plugins, services, AA protocol handlers, External API, codecs, video, EQ, media player).
- Fixtures live in `tests/data/`.
- New tests register in `tests/CMakeLists.txt`.

## Host-runnable vs hardware-dependent

Everything in the suite runs on the WSL2/dev host — protocol handlers are exercised against fakes/loopbacks, audio paths against stub engines. What the suite can NOT prove: real Bluetooth pairing/HFP behavior, PipeWire on-device routing, touch hardware, and phone-side AA quirks — those need the Pi + a phone (bench checklists live in the relevant plan/handoff docs).
