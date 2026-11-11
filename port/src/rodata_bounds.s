; SPDX-License-Identifier: Apache-2.0
;
; cbprintf decides whether a format string may be referenced in place by
; checking the pointer against the read-only region. This runtime maps the
; whole 1 MiB flash window at 0x08000000, so those are the bounds.

        AREA |.rodata_bounds|, DATA, READONLY
        EXPORT __rodata_region_start
        EXPORT __rodata_region_end

__rodata_region_start EQU 0x08000000
__rodata_region_end   EQU 0x08100000

        END