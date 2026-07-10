## Summary

<!-- What changed, in a sentence or two. -->

## Why

<!-- The problem this solves or the feature it adds. Link issues. -->

## Testing

- [ ] Local build (incl. app target: `cmake --build . --target openauto-prodigy`)
- [ ] `ctest --output-on-failure`
- [ ] Cross-build (`./cross-build.sh`)
- [ ] Verified on Pi hardware
- [ ] Not applicable (docs-only, etc.)

```bash
cmake --build . && ctest --output-on-failure
```

## Deployment notes

<!-- Config changes? systemd/service changes? install.sh changes? "None" is fine. -->

## Risk

<!-- What could break, and how would we notice? -->
