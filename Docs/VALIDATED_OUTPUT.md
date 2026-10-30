# Validated output publication — Issue #14 (FR-OUT-01 / TC-09)

The existing controller stages payload/archive/restored bytes in a temporary
directory under the destination parent. It closes streams and, on decompression,
checks decode success, the exact archive boundary and restored size/CRC before
publishing the final filename through a non-overwriting hard link.

This change retains that design and adds sanitized Result-based handling for
standard exceptions crossing the compression/decompression boundary. Scoped
cleanup now runs while unwinding those failures, rather than allowing an
unhandled codec/filesystem exception to terminate the caller. Failed operations
reset statistics and do not disclose exception text or claim success.

`team14.output_publication_faults` links the real controller, archive serializer,
file checks and validator to a **test-only single-symbol codec substitute**. It
injects partial writes, returned errors, bad stream state, thrown standard
exceptions, too-short decoded output, incorrect decoded CRC and archive
truncation/trailing bytes. It observes that the final pathname is absent during
encoding/decoding, verifies no final output on failure, unchanged inputs, reset
statistics and no remaining staging/access-probe directories. Success fixtures
must be published only after their real size/CRC checks pass.

The production huf binary still links the real codec, which is exercised by the
existing codec, bit-I/O, round-trip, controller, output-limit and CLI suites.
No test fault controls, shared declarations, archive-format changes or alternate
publication fallback are added to production.

This is failure handling for normal returns and standard C++ exceptions. It is
not a claim of crash/power-loss recovery, protection against a hostile process
with equivalent filesystem access, or reliable cleanup when the OS refuses to
remove files. Atomic no-overwrite publication still requires filesystem hard-link
support. Hardware/disk-full behavior is simulated through failed stream states,
not by filling a real disk.
