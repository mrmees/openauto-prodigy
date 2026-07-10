# tools/

Protocol analysis and release tooling. Everything here is safe to re-run; generated outputs are marked below.

## AA proto graph

- **`aa_proto_graph.py`** — walks the proto definitions and generates the protocol reference: `docs/aa-protocol/protocol-reference.md` (untracked, gitignored — regenerate on demand) plus `aa_proto_graph.json` (tracked snapshot).

## Proto validation chain

Input: proto sources + APK-extracted descriptors. Run order matters only for full refreshes; each script is idempotent.

- **`proto_parser.py`** — parses `.proto` files into a comparable model.
- **`descriptor_decoder.py`** / **`proto_decoder.py`** — decode APK-embedded descriptor sets / raw proto payloads.
- **`match_loader.py`** — loads message-match definitions.
- **`proto_comparator.py`** — compares our protos against APK descriptors.
- **`validate_protos.py`** — drives the chain; writes the reports.
- Generated outputs: **`proto_matches.json`**, **`proto_validation_report.json`** (tracked snapshots).
- **`test_*.py`** — pytest suite for the chain (`python3 -m pytest tools/`).

## JS / web

- **`gen-proto-js.sh`** — generates the JS proto bindings for web widgets.
- **`proto-usage-report.sh`** — reports which proto messages the codebase actually uses.
- **`token-preview.sh`** — previews theme token output.

## Release

- **`package-prebuilt-release.sh`** — packages a prebuilt Pi release into `dist/` (gitignored).

## Spike dirs

- **`eme-probe/`**, **`spike-qmp-tap/`** — exploratory spikes kept for reference; not part of any build.
