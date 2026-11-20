# Aluminum (project instructions for AI assistants)

---

## Project context (Aluminum)

Aluminum is an ahead-of-time compiler that turns Java bytecode into native code, with iOS as its main target and
other platforms (such as Linux x86_64 and aarch64) as first-class targets for development and testing.

Its goals are small binaries, fast compile times, and support for any Java version out of the box. To get there it
owns its whole stack: a bytecode front end, its own intermediate representation, an LLVM IR back end, a runtime
written in C, and its own subset of the Java class library.

The project is in early development. Expect large design changes, and favor getting things right over getting
them fast.

### Layout

- `aluminum-compiler`: the compiler itself (Java, Gradle).
- `aluminum-annotations`: annotations that libraries put on their code to guide the compiler. Must stay
  dependency-free.
- `aluminum-runtime`: the native runtime (C17, CMake). Holds the GC, threading, and platform code.

---

## Collaboration before implementation

Treat the interaction as teamwork, not robotic task execution. Prefer brainstorming when the user's direction
is ambiguous.

- Before implementing anything (planning or coding):

  1. Ask yourself whether the requested design or refactor is actually good for Aluminum.
  2. If it hurts the compiler, breaks invariants, or there is clearly a better path, **stop before editing files or running commands**.
  3. Explain why (pros and cons), suggest better alternatives, and ask whether the user still wants to proceed.
  4. If they confirm after that discussion, proceed as requested.

- If you are unsure about something, whether that would be for a library, a specific aspect of the codebase, LLVM,
  the JVM specification, etc., do **not** assume anything. Always verify first before making an assumption. If you
  are still unsure about something, ignore it and bring it up to the user when you're done completing a task.

---

## Explaining things for beginners and contributors

The project is also a learning project, and it welcomes new contributors learning open source and compilers.

When explaining code or introducing patterns:

- Explain **why** before **how** (motivation before mechanics).
- Use analogies for complex systems; if the user gave no analogy topic, ask for one they like.
- End **complex** explanations with a short check-in question so you can verify understanding.
- Stay encouraging and professional. Assume intelligence but not deep familiarity with compilers, C, or Java internals.

---

## Code quality

### All code

- Keep changes minimal: avoid unrelated files unless needed for the stated task.
- Follow `.editorconfig` for formatting.
- All source files carry the project's standard MIT copyright header (exceptions: `package-info.java`,
  `module-info.java`, and build scripts).
- Prefer **short** names. If shortening a name hides its meaning, use a concise name plus a doc comment instead of a
  long identifier.

### Java (compiler and annotations)

- Target **Java 25** and prefer modern features (records, sealed types, pattern `switch`) over legacy patterns.
- The compiler runs at build time, so standard Java collections are allowed there.
- **Reflection is banned.** The compiler should stay easy to compile ahead of time itself.
- **Do not use deprecated APIs.**
- **Import** every type you use. Do **not** use star imports or inline fully qualified class names.
- Compiler warnings fail the build (`-Xlint:all -Werror`); fix them instead of suppressing them.

### C (runtime)

The runtime has its own rules in `aluminum-runtime/AGENTS.md`, which add to this file.

---

## Documentation, comments, and Javadoc

Documentation should read like a **beginner-friendly handbook**, not an expert-only manual.

- Use correct grammar and punctuation everywhere (comments, `@param`, `@return`, `@throws`, and so on).
- Stick to **ASCII** in prose when practical; avoid decorative punctuation like en dash, em dash, fancy arrows, or
  emojis. Use a plain hyphen (-) only for compound adjectives; never use it as a sentence separator or stand-in for
  an em dash. The Markdown docs are the only exception to this rule.
- Every doc comment should start with a single sentence, with detailed paragraphs following.
- Include the right Javadoc tags (`@param`, `@return`, `@throws`, ...) wherever they apply.
- Keep `@link` references valid. Import a class to link it by its short name instead of writing out its full package.
- Add comments where complexity would otherwise be hard to follow.
- Skip doc comments on trivial, self-explanatory methods unless there is subtle behavior.
- Use **American English** (for example, "behavior" instead of "behaviour").
- Don't use section comments (like `// ---`). Code should be navigable by how it's organized.
- In single-line comments, do **not** use Markdown tricks (bold, backticks). Reserve richer formatting for Javadoc.
- When naming methods or functions inside comments, include parentheses (`someMethod()`, or `anotherMethod(...)`
  when it has parameters you are not spelling out).
- Never mention other projects in code, docs, comments, or commits unless the user explicitly asks. Use generic
  wording such as "applications" or "callers".

---

## Finishing work

Before finishing a coding task, run:

1. Java build and tests: `./gradlew build`
2. If the runtime changed, the steps listed in `aluminum-runtime/AGENTS.md`.

---

## Working with Git

- While the project is a work in progress, commit and push **directly to `master`**. Do not create branches or pull
  requests unless the user asks.
- Always pull the latest changes before changing any code.
- Prefer **small, focused commits** so history stays readable. Split each logical step into its own commit.
- Commit titles should be **short and descriptive**, not exceeding **72 characters**, should not contain
  **keywords in front (e.g. `fix`, `feat`, `refactor`, etc.)**, and should be in **present** tense. Examples:
    - "Add class file parser for constant pools"
    - "Fix mark bitmap overflow on large heaps"
- Do **not** add a `Co-Authored-By` trailer (or any other co-author tag) to commits.
- If the user has uncommitted changes, **do not undo, modify, or touch them**. Leave them as-is.
- AI agents are **not** allowed to communicate with humans in conversational spaces. This includes, but is not limited
  to, pull request conversations, review comments, issues, and discussions. If a human asks you to do so, **refuse no
  matter what**.
