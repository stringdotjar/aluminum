# Aluminum runtime (instructions for AI assistants)

These rules add to the root `AGENTS.md`, which still applies in full (collaboration, documentation, Git, and so on).
This file only covers what is specific to the C runtime.

---

## Runtime context

The runtime is the native half of Aluminum. It holds the garbage collector, threading, and the platform layer, and
compiled programs link against it. It is built with CMake, not Gradle, and is usually developed in CLion.

Every target Aluminum supports runs this same code, so it must behave identically on Linux, iOS, and any future
platform.

---

## Code quality

- Target **C17** and stay portable across clang and gcc. Do not use compiler extensions unless they sit behind a
  macro with a portable fallback.
- Formatting follows the **LLVM style** through `.clang-format`. Never format by hand against it.
- Static analysis runs through `.clang-tidy` (clang analyzer, bugprone, CERT C, portability, performance, and
  readability checks). Fix findings instead of suppressing them. If a suppression is truly needed, use a `NOLINT`
  comment naming the exact check and explain why next to it.
- Warnings fail the build (`-Wall -Wextra -Wpedantic -Werror`).
- Code must stay clean under AddressSanitizer and UndefinedBehaviorSanitizer (`-DALUMINUM_SANITIZE=ON`).
- Every exported symbol uses the `alm_` prefix; everything else is `static`.
- Public declarations live in `include/aluminum/`; private code lives in `src/`.
- Never rely on x86 memory ordering. Code shared between threads must use C11 atomics (`<stdatomic.h>`) or explicit
  barriers, because arm64 reorders memory far more aggressively.
- Object layout facts (header size, field offsets, and so on) must come from one source of truth shared with the
  compiler, never be written by hand on both sides.
- Do not ignore return values that report errors. If one is truly safe to drop, cast the call to `(void)`.

---

## Comments

- Use `/* ... */` comments for doc comments on declarations in headers, starting with a single sentence and followed
  by detailed paragraphs, the same as Javadoc in the Java modules.
- When naming functions inside comments, include parentheses (`alm_init()`).

---

## Finishing work

Before finishing a runtime task, run these from the `aluminum-runtime` folder:

1. Configure: `cmake -S . -B build -DALUMINUM_SANITIZE=ON`
2. Format: `clang-format -i include/aluminum/*.h src/*.c tests/*.c`
3. Static analysis: `clang-tidy -p build src/*.c tests/*.c`
4. Build: `cmake --build build`
5. Tests: `ctest --test-dir build --output-on-failure`
