# Exact calendar ring octants

The two 240 x 240 calendar ring tables contain 11,140 unique pixel/angle/alpha
tuples. Integer reflection and diagonal symmetry reconstruct every tuple exactly
from 1,399 octant tuples. The generator rejects any input whose symmetry is not
exact. Original calendar_assets.inc and its generator remain unchanged.

The compact renderer omits diagonal duplicate transforms, blends each pixel once,
and retains the existing angle boundary and endpoint-cap behavior. Other display
sizes use the unchanged procedural renderer. The 512-byte atlas codec is unchanged.

Stored ring arrays shrink from 66,840 to 8,394 bytes. The actual common Home ELF
shrinks from 610,872 to 552,616 bytes, saving 58,256 bytes after code/link effects.
Two independent GCC15 builds produce the identical SHA-256 recorded in the JSON
receipt, with unchanged feature definitions, imports, manifest and dependency pins.
The shared build has no board/profile switches and is suitable for separate
T-Watch and Waveshare product qualification. Existing product pins stay immutable.

The production decoder uses one six-byte output tuple and scalar local arithmetic;
it adds no heap or persistent workspace. Each base tuple attempts eight bounded
transforms, skipping swapped diagonals. No service or I/O call is added. Hardware
rendering latency is not measured.

The normal and ASan/UBSan tuple tests check all original values, uniqueness,
diagonal handling and invalid bounds. The full 1,632-frame/eight-size matrix
compares original uncompressed tables against optimized rendering both normally
and under ASan/UBSan. The frame stream remains exactly the previous SHA-256.

A Waveshare diagnostic composition over exact released 0.1.54 contains 132 files
and 64 ELFs, retaining every non-Home ELF. With an explicit internal-only core
feed (zero optional mirrors), it uses 20,307 SPIFFS pages with 93 free pages and
six fully free blocks, exceeding the required four-block reserve. Final product
source identity, firmware, admission and cumulative Wi-Fi baseline are separately
qualified before delivery. No product native firmware or hardware claim follows
from this shared-source change.
