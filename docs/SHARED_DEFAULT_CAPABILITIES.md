# One canonical default application

Home has one target build. There is no board, display-resolution, preference-backend, or export-volume build parameter. Products package the same `default.elf` and `default.json`.

## Display

The existing native surface path remains unchanged: dimensions and touch coordinates come from capabilities. Layout supports the qualified 240×240 and 410×502 watches and the wider multi-resolution test matrix. Reference design units are not a fixed physical framebuffer.

## Preferences

Home and Quick use the namespace-bound `storage.preferences@1` provider. The app never selects a persistence mechanism. Providers report copied-input and atomic-group operation semantics explicitly. Existing keys and encodings are preserved. A sequential provider writes restore-level before a paired zero-level preference; a partial accepted failure is uncertain and is never automatically replayed. Missing, malformed, busy or retained providers do not trigger a different-backend fallback.

## Files

Crash export optionally acquires a unique authorized `storage.files@1` namespace. Home uses ordinary checked file CRUD; media, transport, power and physical provider identities remain behind providers/provisioning. The internal namespace-61 spool remains authoritative until exact final-file verification. Missing or unavailable optional file storage leaves it pending. Retained custody is terminal for the invocation.

`storage.files.catalog@1` is available to applications that need discovery of multiple authorized namespaces; Home does not need to enumerate. Neither generic files nor preferences imply mandatory storage on every RiscRTE product. Storage-using products opt in and provision appropriate providers. A storage-free product does not need this watch Home application or its storage capabilities.

## Qualification

The generic preference harness uses the same compiled Home/Quick binary against synchronous and atomic providers, including failure paths. Native face, viewport and input suites cover multiple physical dimensions. Crash-session tests cover absent/malformed/unavailable/retained providers and the durable-spool, presentation, native-ack and final-export ordering.

`verify_shared_default.py` compares two independent builds and their source/toolchain receipts, rejects legacy backend capability strings/declarations, and can extract both product boot stores to require byte-identical canonical ELF and manifest files. Product release qualification must invoke its two-store check. Software qualification does not establish hardware acceptance.
