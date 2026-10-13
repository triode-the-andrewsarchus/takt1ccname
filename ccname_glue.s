| takt1ccname: glue for SETTINGS menu integration
| Firmware addresses for OS 1.53 and OS 1.54

        .ifdef  OS154
        .equ    .LF_4017ac20, 0x4017af20
        .else
        .equ    .LF_4017ac20, 0x4017ac20
        .endif

        .text

| ---------------- SETTINGS: the MIDI CC NAMES row --------------------------------------------------
        .globl  ccname_settings
ccname_settings:
        pea     row_ccname
        move.l  8(%sp), -(%sp)                  | the menu
        jsr     core_additem
        addq.l  #8, %sp
        rts

| label: builds the std::string in a0
row_label:
        link    %a6, #-4
        move.l  %d2, -(%sp)
        pea     -1(%a6)
        pea     str_row
        move.l  %a0, %d2
        move.l  %a0, -(%sp)
        jsr     .LF_4017ac20                      | std::string(this, const char*, alloc&)
        lea     12(%sp), %sp
        move.l  %d2, %d0
        move.l  -8(%a6), %d2
        unlk    %a6
        rts

        .balign 4
row_ccname:
        .long   row_label, ccname_select, ccname_rowdraw, ccname_change
str_row:
        .asciz  "MIDI CC NAMES"
        .balign 2
