Notes for later (enable DNSSEC on Windows)

- We stubbed DNSSEC-related includes to progress SyncRes build:
  - Disabled DNSSEC via compile definition `HAVE_DNSSEC=0`.
  - Guarded inclusion of `dnsseckeeper.hh` in `syncres.cc`.
  - Missing upstream deps observed when enabling: `ueberbackend.hh` -> `dnsbackend.hh` (auth backend interfaces).

- When we re-enable DNSSEC:
  1) Set `HAVE_DNSSEC=1` in the CMake target.
  2) Vendor or reference upstream headers/sources:
     - `dnsseckeeper.hh` (+ any .cc if used)
     - `ueberbackend.hh` and its dependency `dnsbackend.hh` (and minimal transitive deps)
  3) Ensure OpenSSL linkage is present (already wired) and any runtime knobs are set.

- Rationale: keep the Windows POC minimal and unblock SyncRes; revisit once core build is green.

Logging writev/WSASend optimization (To Revisit)
------------------------------------------------

- For Windows POC, `remote_logger.cc` avoids POSIX `writev` by coalescing buffers into a single `write()` when `_WIN32` is defined. Upstream uses `writev` (via `<sys/uio.h>`) to batch multiple buffers.
- Follow-up to improve:
  - Replace coalescing fallback with `WSASend` and an array of `WSABUF` when the destination is a socket to preserve batching and reduce syscalls.
  - Keep a portable shim for non-socket FDs (looped writes or `WriteFile` with a single coalesced buffer) if ever needed.

ZONEMD Support (Deferred)
-------------------------

- For the Windows POC, `zonemd.cc` is excluded. ZONEMD is a zone-integrity feature used mainly in authoritative/zone-distribution workflows and is not required for recursive resolution.
- To re-enable later:
  - Include `zonemd.cc` in the CMake sources.
  - Ensure `sha.hh` (and any minimal hashing dependencies used by ZONEMD) are available or wired in.

Upstream main wiring (`pdns_recursor.cc`) and temporary globals stub
-------------------------------------------------------------------

- For the Windows POC we excluded `pdns_recursor.cc` (it provides `main()` and daemon wiring, including `udp_handler`) to keep dependencies minimal and avoid conflicts with our `main_test.cc`.
- To satisfy linker references without pulling the full daemon, we added `pdns_recursor_windows/globals_stub.cc`, which defines the minimal set of global symbols used by `SyncRes`/`lwres`/caches.
- Migration plan to enable upstream wiring later:
  1) Remove `globals_stub.cc` from the build.
  2) Drop `main_test.cc` from the target.
  3) Add `pdns-recursor/pdns_recursor.cc` and any required upstream sources it pulls transitively.
  4) Ensure Windows guards for sockets/timers are satisfied (Winsock init, `FDMultiplexer` implementation already present).
  5) Re-run the build and address any remaining platform guards (Lua/DNSSEC/FSTRM/etc.) as we progressively re-enable features.
