# Overwrite protection and failure cleanup — Issue #21

NFR-SAFE-01 / TC-09: existing outputs must remain unchanged by default, and
failed writes/restorations must not leave a trusted partial output.

The necessary production safeguards already exist in the merged file I/O and
controller implementations (including Issue #14). This issue completes their
end-to-end and race-window regression coverage rather than replacing working
teammates' code or duplicating publication logic.

## Behavior verified

- Compression and decompression refuse occupied destinations by default.
  There is no CLI overwrite/force option; refusing replacement is intentional.
  The low-level `validateOutputFile(..., true)` remains only a preflight API,
  not a grant to overwrite during controller publication.
- Existing regular files, original input/archive paths, directories, hard-link
  aliases, output symlinks and dangling symlinks are preserved.
- Existing data stays intact even if a new file, directory or symlink appears
  **after** preflight but **before** publication. The real hard-link publisher
  refuses the collision; it never falls back to overwriting rename/copy.
- Bad CRC, nonzero padding, truncation and trailing-byte failures do not publish
  restored output. Inputs remain unchanged, statistics reset, staging/access
  probes disappear and a subsequent valid retry can use the same destination.
- Injected late output-stream failures leave no final output or staging files.
- CLI errors return exit 1 with diagnostics and no misleading success message;
  original/source/archive SHA-256 hashes remain unchanged.

## Tests

- `team14.overwrite_cleanup` uses the real codec/controller/validator for normal
  operations and corruption/permission failures.
- `team14.no_overwrite_races` links the real controller, archive, file checks and
  validator to a test-only single-symbol codec. It deterministically introduces
  competing filesystem entries between preflight and publication. There are no
  sleeps, flaky thread timing, or production fault-injection switches.
- `team14.overwrite_cli` exercises the real huf executable and compares hashes.

Build and run through CTest as documented in `BUILDING.md`. Tests report skips
when symlink/hard-link fixtures are unavailable or the account/OS bypasses
permission bits. Run the API suite unprivileged to exercise denied-write cases;
a permission skip must not be reported as a verified denied-write case.

## Scope and limits

No source implementation, shared interface, archive format, or existing test
was changed. Build changes register only these additional regression targets.
This is not crash/power-loss recovery, adversarial same-privilege isolation,
secure erasure, or a guarantee that cleanup works when the OS refuses removal.
Publishing still requires same-filesystem hard-link support; disk-write errors
are injected through stream state rather than filling a real disk. Platform
execution evidence belongs in the PR; a Linux run does not certify Windows or
macOS. Temporary files are untrusted staging, never accepted final outputs.
