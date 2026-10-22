# Upstream provenance

- Development base: https://github.com/MYdesignTool/AE-UTF8-Fixer-Ae2024
- Base commit: ef288e5f8021a0ea0fb9c7dd03f188aa049ba476
- Original author and initial core implementation: **AdoreLowen**, https://github.com/AdoreLowen/AE-UTF8-Fixer.
- Original MIT license and copyright retained in LICENSE.
- MinHook: https://github.com/TsudaKageyu/minhook, v1.3.4, commit c3fcafdc10146beb5919319d0683e44e3c30d537.
- MinHook license retained in third_party/minhook/LICENSE.txt and copied into runtime packages.
- Local compilation uses the user-supplied May2023_AfterEffectsSDK/Examples, outside this repository. The runtime package contains no Adobe SDK files.

Changes in 0.2.0: English-UI gate, product-version detection, configurable legacy text decoder bridge, strict encoding checks, bounded string conversion, typed effect/registration wrappers, validated PiPL parsing, independent trampolines for each host add_param callback, dynamic UI repair without writing host-owned parameters, process-lifetime hook storage and a mock-host test suite.

Changes in 0.2.1: prevent lossy ANSI re-encoding of registered Chinese display text during popup processing, preserve lossless requested encodings and explicit Unicode calls, add popup placeholder diagnostics and menu round-trip regressions. These are derivative changes, not a claim to authorship of the initial project. The original copyright notice, Git history and AL's Toolkit PiPL category are retained.

Changes in 0.2.2: mixed-menu encoding handling, Options/effect-title/About/error text coverage, failed-hook pointer rollback and wrapper-pool accounting, deterministic input/API stress tests and expanded mock-host regressions. Original author and dependency ownership remain as above.

Changes in 0.2.3: default Chinese and English AE 2024+ activation, bounded language detection, pre-2024 bridge exclusion, isolated per-case configuration tests and Chinese/Western code-page mock-host coverage. Maintained by MYdesignTool with original author attribution and license retained.
