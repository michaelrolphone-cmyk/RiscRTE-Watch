# Lossless atlas block-size qualification

This change preserves the exact decoded face/calendar alpha bytes, glyph tables,
rendered pixels, public APIs and feature selections. It changes independently
compressed block size from256 to512 bytes, retaining the existing one-byte
backreference distance. The encoder must stop searching beyond255 bytes; no token
changes its meaning. Source atlases and immutable prior recipe pins are unchanged.

The packed face atlas plus offsets falls124940→117615 bytes; calendar93162→85764.
Total source asset saving is14723 bytes. Decoder scratch rises256→512 bytes per
call; existing glyph caches remain6853/9858 bytes. There is no heap allocation,
persistent history or service I/O. A block decode produces at most512 bytes and
consumes at most1024 encoded bytes. For a requested range, decoded work is bounded
by512×ceil((start%512+count)/512), versus the corresponding256-byte bound before.
This bounds the additional edge-block work; it is not a hardware latency claim.

Two independent GCC15.2.0 target builds at this source produced the same610872-byte
Home0.19 ELF: b5d8d7b545da737879a1d369160cd4c6eea4cee8876d09ce783cf00560f70231.
The preceding selected artifact was625648 bytes, giving14776 bytes of actual ELF
savings. Allocated BSS remains265772 bytes and data28 bytes; rodata falls14720
bytes, text falls28, relocation tables grow72 and got.loc grows16. This leaves
public imports/exports and feature definitions unchanged. Product adoption is
qualified separately against each complete store and native firmware graph.

Tests compare every original byte, every glyph and513-byte ranges at every start
offset, normally and under ASan/UBSan. Synthetic codec cases include overlap,
malformed tokens/indexes, full512-byte blocks and distances255/256. The rendering
matrix compares1632 complete face/state frames across eight native sizes; exact
hash-stream SHA256 is2cd93ee0909fae30b9e393a025b0e2cc23c96a328481cca62973bff4a554b000.
The ordinary Home lifecycle, provisioning, first-frame, sleep-refusal, layout and
service-checkpoint suites remain required, as does the existing twice-built
shared-default verifier. Physical timing and first-start acceptance remain hardware
checks.
