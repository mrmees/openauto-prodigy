PYTHONPATH := .
SOURCE ?=
ANALYSIS_ROOT ?= analysis

.PHONY: index
index:
	@if [ -z "$(SOURCE)" ]; then \
		echo "Usage: make -C analysis/tools/apk_indexer index SOURCE=<decompiled-apk-root> [ANALYSIS_ROOT=analysis]"; \
		exit 1; \
	fi
	cd ../../.. && PYTHONPATH=$(PYTHONPATH) python3 analysis/tools/apk_indexer/run_indexer.py --source "$(SOURCE)" --analysis-root "$(ANALYSIS_ROOT)"
