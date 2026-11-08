@ Embeds the original game resource files into the ARM9 binary.
@ GAMEDIR is passed on the command line (-DGAMEDIR=...).
#define STR2(x) #x
#define STR(x) STR2(x)

    .section .rodata.gamedata, "a"
    .balign 4

    .global res_map_data, res_map_end
res_map_data:
    .incbin STR(GAMEDIR/resource.map)
res_map_end:

#ifndef CDROM   /* the CD volume is read from NitroFS instead */
    .balign 4
    .global res_vol1_data, res_vol1_end
res_vol1_data:
    .incbin STR(GAMEDIR/resource.001)
res_vol1_end:
#endif

#ifndef CDROM
    .balign 4
    .global res_vol2_data, res_vol2_end
res_vol2_data:
    .incbin STR(GAMEDIR/resource.002)
res_vol2_end:
#endif
