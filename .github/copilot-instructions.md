## GitHub Copilot instructions for the "esp32-ace-pro-dryer" repository

Purpose
- Provide concise, actionable guidance for GitHub Copilot and other assistant agents working in this repository. The project is firmware for an ESP32 (ESP32-WROOM-32) controlling an Anycubic ACE Pro filament dryer device through USB, and a few peripherals — safety and non-blocking behavior are paramount.
- Keep this file up-to-date when the board profile, pin wiring or library versions change.

Project summary (short)
- Platform: ESP32 (ESP32-WROOM-32 profile is used).
- Main sketch: `esp32-ace-pro-dryer.ino`.
- Key hardware: Anycubic ACE Pro filament dryer (connected through USB), an SSD1306 display and a KY-040 rotary encoder.
- Important config: `config.h` defines pin mappings.
- Libraries (versions in `sketch.yaml`): Adafruit BusIO, Adafruit GFX Library, Adafruit SSD1306.

Read first
- `esp32-ace-pro-dryer.ino` — main program flow and non-blocking loop structure.
- `config.h` — authoritative pin mapping and defaults. Any change to pins or defaults requires explicit confirmation from the repo owner.
- `sketch.yaml` — board FQBN and library versions used to build.

How to behave (high level)
- Prioritize safety: do not make changes that could cause continuous or extreme heat generation or otherwise stress the hardware.
- Preserve non-blocking behavior: avoid introducing `delay()` or other blocking calls in the main loop. If a longer operation is needed, break it into small steps or use asynchronous/event-based approach.
- Keep edits small and focused: one logical change per PR. Provide a "How I tested" section in the PR describing build results and manual hardware checks, when applicable.
- Use the repository's style: `inline constexpr` for constants and `F("...")` for constant Serial strings.

Operating constraints and safety
- This is firmware for an ESP32 running a filament dryer and heater and external hardware. Avoid changes that can cause permanent hardware damage (e.g. heat).
- The main loop intentionally avoids `delay()` to keep event handling responsive — preserve that approach.
- Serial output is used for debugging; keep helpful prints under `#ifdef` or guard them when making noisy changes.
- Changing pin assignments requires updating `config.h` and documenting the change in the PR.

Coding style & best practices (repo-specific)
- Use `F("...")` for constant strings sent to `Serial` (already used in the code).
- Prefer `inline constexpr` constants (pattern used in `config.h`).
- Keep heavy operations off the main loop or run them in small incremental steps (non-blocking pattern). If a long operation is required, document why and how interrupts and button handling are preserved.
- Avoid dynamic allocations in tight loops; prefer stack or static objects for resource-constrained ESP32.

Allowed changes without explicit owner confirmation
- Documentation, README improvements, and tests that don't alter hardware wiring or runtime behavior.
- Small refactors that do not change external behavior (visible state, timing, servo ranges, pin assignments).

Ask before changing
- Any modification to `config.h`.
- Changing heating limits or any code that alters the physical environment.
- Upgrading or changing library versions in `sketch.yaml`.
- Changing behavior that affects power-up or reboot.

Tone & response format for Copilot
- Be concise and action-oriented. When proposing code changes, provide unified diffs or small patch-style modifications suitable for code review.
- For behavioural changes, list risks, required manual checks, and fallback plans (how to revert if hardware acts unexpectedly).

Common pitfalls to check
- Blocking delays in `loop()` and anywhere event-handling is used.
- Unchecked assumptions about I2C device presence.

Build and CI
- When a build-related change is made (library versions, board profile), ensure the `compile.yml` workflow is updated accordingly.
