# Changelog

The initial AE-UTF8-Fixer implementation is by AdoreLowen. This fork preserves the original MIT notice and history; entries below describe derivative work.

## 0.2.2 — 2026-10-22

- Repair mixed GBK/UTF-8 popup lists item by item when UTF-8 translation items are detectable; preserve the existing all-GBK path, GBK trail bytes equal to 0x7C, empty items, separator tokens, menu count and IDs. Register valid items even when another item is undecodable.
- Cover PF_EffectUISuite1 Options button labels, sequence setup/resetup effect titles, About text and flagged error messages. Preserve SDK return codes, error flags, parameter data and normal render-path outputs.
- Make hook installation transactional: restore forwarding pointers before freeing failed trampolines, remove failed registrations and avoid consuming wrapper slots on failed installs.
- Reproduce the mixed-menu and output-title omissions on the 0.2.1 implementation before fixing them. Add deterministic malformed-input/native-API tests, failure injection and broader mock-host text checks.
- Share SDK text-field aliases between production code and test fixtures; verify builds with both the supplied May 2023 SDK and repository SDK, including cross-loading the resulting mock hosts and repair binaries.
- User retested this version in English AE 24.6.2 and confirmed the tested text, including the previously affected popup options, displays correctly. Other host versions and untested plugins still require real-host verification.
- Keep original AdoreLowen attribution, MIT notice and history unchanged.

## 0.2.1 — 2026-10-22 (candidate)

- Address the reported question-mark popup items in English AE 24.6.2 by covering a previously unprotected encoding round-trip. The decoder-only 0.2.0 binary fails the new mock-host regression; this version passes it.
- Register Unicode forms of known Chinese display text and popup items. Redirect WideCharToMultiByte only when the requested legacy code page would lose those registered characters. Keep lossless legacy conversions, unregistered text, explicit Unicode calls and Win32 buffer-size/error behavior.
- Use original encoder trampolines for the fixer's own GBK round-trip validation, avoiding self-interception.
- Diagnose placeholder-only popup input without inventing replacement labels. Preserve separator tokens, menu count, order, values and IDs.
- Add 14-item popup regressions, buffer and passthrough checks, concurrency checks and the full-DLL mock-host round-trip.
- Retain original author attribution, copyright, history and AL's Toolkit metadata category.

The supplied P_Fill sample contains English labels; the Chinese-patched variant in the screenshot was not supplied. The exact screenshot outcome still needs user verification. No commercial plugin binary is distributed.

## 0.2.0 — 2026-10-22 (candidate)

- Add an English-UI and AE 24+ product-version gate, strict GBK/UTF-8 handling and a configurable legacy decoder bridge.
- Add typed effect/registration wrappers, bounded PiPL parsing, per-target host callbacks, SDK-safe parameter copies and dynamic UI name repair.
- Add local SDK builds, pinned MinHook sources, mock-host tests and runtime packaging.
