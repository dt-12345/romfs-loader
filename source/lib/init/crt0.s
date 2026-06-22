.section ".text.crt0","ax"

.macro FROM_MOD0 register_num, offset
    ldr w\register_num, [x24, #\offset]
    sxtw x\register_num, w\register_num
    add x\register_num, x\register_num, x24
.endm

.macro FUNC_RELATIVE_ASLR name, register_num, symbol
.word \symbol - .
\name:
    ldr w\register_num, [x30]
    sxtw x\register_num, w\register_num
    add x\register_num, x\register_num, x30
.endm

.global __module_start
__module_start:
    // newer rtld versions check the first four bytes to determine the version
    // 0, `b #0x8` (arm), or `b #0x8` (aarch64) are treated as the older version anything else is treated as the new version
    // older versions of rtld appear to not care about what comes after the module header offset
    .word 0
    .word __nx_mod0 - __module_start
    .word __sdk_version - __module_start

    .align 4
    .ascii "~~exlaunch uwu~~"

.section ".rodata.mod0","a"

.hidden exl_nx_module_runtime 

.align 2
__nx_mod0:
    .ascii "MOD0"
    .word  __dynamic_start__            - __nx_mod0
    .word  __bss_start__                - __nx_mod0
    .word  __bss_end__                  - __nx_mod0
    .word  __eh_frame_hdr_start__       - __nx_mod0
    .word  __eh_frame_hdr_end__         - __nx_mod0
    .word  exl_nx_module_runtime        - __nx_mod0
    .word   __relro_start__             - __nx_mod0
    .word   __full_relro_end__          - __nx_mod0
    .word   __module_name_start__       - __nx_mod0
    .word   __module_name_end__         - __nx_mod0
    .word   __note_gnu_build_id_start__ - __nx_mod0
    .word   __note_gnu_build_id_end__   - __nx_mod0

// this will only be checked if something throws an exception
__sdk_version:
    .word   20
    .word   5
    .word   6