# Tony status panel example

This folder preserves the standalone Qt `TonyStatusPanel` contributed during early Desktop UI experiments.

It is intentionally kept under `desktop/qt/examples/` because it is **not part of the current Superpower Desktop build target**. The production Desktop workspace is defined in `desktop/qt/src/` and `desktop/qt/CMakeLists.txt`.

Keeping optional experiments here prevents unreferenced C++ files from cluttering the repository root while preserving the implementation for future reuse.
