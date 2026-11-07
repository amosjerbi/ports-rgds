; ---- Donkey Kong (GB) CGB colourisation hooks (own code; SM83) ------------------------------------
; The game already sends Super Game Boy colour packets. On a CGB we pretend an SGB is present,
; swallow the packets ($3872) and replay the colour they describe with CGB palettes/attributes:
;   - 4 SGB palettes (PALS) re-derived from BGP/OBP0/OBP1 every frame (so fades keep working)
;   - 20x18 screen-cell attribute map (MAP) -> BG tile attributes (VRAM bank 1, following SCX/SCY)
;     and OBJ palette bits per sprite (palette of the cell under the sprite's centre)
; Interrupt vectors are wrapped so handlers always see WRAM bank 1 (the main thread may sit in bank 4).
; ALL variables and arrays live in WRAM bank 4 (the game never uses it; its bank-1 area $d7xx is NOT free in every scene)
; interrupt wrappers force SVBK=1 for the game handlers, our code sits in bank 4 and only the RAM packet read uses bank 1.
; bank 4
QHEAD = $df20   ; packet ring head
QTAIL = $df21   ; packet ring tail
QBUF  = $df22   ; 8 entries
UPL   = $df2b   ; bit0 BG palettes pending upload, bit1 OBJ palettes pending
GD    = $df2c   ; attribute GDMA slice 0-3 pending, $ff idle
GDB   = $df2d   ; high byte of the BG map to copy to ($98/$9c), 0 = none
RB    = $df2e   ; saved ROM bank
DPEND = $df2f   ; 1 = DPKT pending
DPKT  = $df30   ; copy of the last RAM-built PAL_SET packet (10 bytes)
FLG   = $df00   ; bit0 palettes changed, bit1 map changed
CBGP  = $df01   ; cached BGP, OBP0, OBP1
CSCX  = $df04   ; cached SCX>>3
CSCY  = $df05   ; cached SCY>>3
CLC3  = $df06   ; cached LCDC bit3
DONE  = $df07   ; bit0 palettes derived, bit1 map built
GDBN  = $df08   ; new GDB value
T0    = $df10   ; temporaries
T1    = $df11
T2    = $df12
T3    = $df13
T4    = $df14
T5    = $df15
PALS  = $d000   ; 4 palettes x 4 colours x 2
MAP   = $d020   ; 20x18 palette numbers
SHBG  = $d200   ; 32x32 BG attribute shadow (4 slices of 256)
CPAL  = $dc00   ; derived CGB palettes: 8 BG (64 B), 4 OBJ(OBP0), 4 OBJ(OBP1) (32 B each)
OVRON = $df40   ; stage colour override active (Big-City 1-2)
HUDW  = $df41   ; window-map row 0 attribute to write at the next idle VBlank (palette+1), 0 = none
BGSP  = $df45   ; 1 = override map is indexed by BG row (scrolling stages), 0 = by screen row
OVRTH = $df47   ; world theme number 1..9 loaded (stage selector 3)
OVRSEL= $df46   ; which stage override is loaded (0 = Big-City 1-2, 1 = scrolling city stage)
BGMAP = $d800   ; 20 x 32 override map in BG coordinates
DKLO  = $df42   ; Donkey Kong sprite tile range [DKLO,DKHI) in the current stage, DKHI=0 none
DKHI  = $df43
DKP   = $df44   ; previous DKHI
T6    = $df16   ; DERIVE source low / high / palette count
T7    = $df17
T8    = $df18
OVRP  = $d600   ; override: 8 BG palettes by shade, then 4+4 OBJ palettes (OVRO = $d640)
OBJCLS= $d700   ; override: sprite tile -> OBJ palette class

.bank 0
.org $390c

; ---- interrupt wrappers: handlers run with WRAM bank 1 ---------------------------------------------
; $ff72 = STAT handler active, $ff73 = VBlank was held back (CGB scratch registers).  The game redraws with a chain of
; scan-line (STAT) interrupts; the wrappers make the last one (line 143) so slow that VBlank would nest into it and the
; chain loses a step (blank background frame).  So VBlank waits until the STAT handler is done.
W40:
    push af
    ldh a,($72)
    and a
    jr z,.go
    ld a,1
    ldh ($73),a
    pop af
    reti
.go:
    ldh a,($70)
    push af
    ld a,1
    ldh ($70),a
    call $0270
    pop af
    ldh ($70),a
    pop af
    reti
W48:
    push af
    ldh a,($70)
    push af
    ld a,1
    ldh ($70),a
    ldh ($72),a
    call $02ad
    xor a
    ldh ($72),a
    ldh a,($73)
    and a
    jr z,.nov
    xor a
    ldh ($73),a
    call $0270              ; the VBlank handler that was held back
.nov:
    pop af
    ldh ($70),a
    pop af
    reti
W50:
    push af
    ldh a,($70)
    push af
    ld a,1
    ldh ($70),a
    call $02c4
    pop af
    ldh ($70),a
    pop af
    reti

; ---- start-up: replaces `call $3718` (SGB detection) ------------------------------------
H_INIT:
    ldh a,($4f)
    cp $fe
    jp nz,$3718            ; DMG / real SGB: original code
    ld hl,$c82b
    set 7,(hl)             ; the game now takes its SGB code path
    ld a,$9c
    ld ($da40),a           ; SGB-path BGP
    ld a,4
    ldh ($70),a
    ld hl,$d000            ; clear bank 4 (palettes, map, shadow, derived, variables)
    ld bc,$1000
.clr:
    xor a
    ld (hl+),a
    dec bc
    ld a,b
    or c
    jr nz,.clr
    xor a
    ldh ($72),a
    ldh ($73),a
    ld a,$ff
    ld (GD),a
    ld a,3
    ld (FLG),a
    ld a,$ff
    ld hl,CBGP
    ld (hl+),a
    ld (hl+),a
    ld (hl),a
    ld (CSCX),a
    ld (CSCY),a
    ld (CLC3),a
    ld hl,PALS             ; default grey palettes until the game sends its own
    ld c,4
.dp:
    ld de,DEFPAL
    ld b,8
.dq:
    ld a,(de)
    ld (hl+),a
    inc de
    dec b
    jr nz,.dq
    dec c
    jr nz,.dp
    ld a,1
    ldh ($70),a
    di
    ret

DEFPAL:
    dw $7fff,$56b5,$294a,$0000

; ---- replaces the 7 bytes at $3872 ( ld b,a / ld a,($c82b) / bit 7,a / ret z ) -------------------
H_SGB:
    ld b,a
    ldh a,($4f)
    cp $fe
    jr z,.cgb
    ld a,($c82b)
    bit 7,a
    ret z
    jp $3879
.cgb:
    push hl
    push de
    ld a,4
    ldh ($70),a
    ld a,(QHEAD)
    ld e,a
    inc a
    and 7
    ld d,a
    ld a,(QTAIL)
    cp d
    jr z,.full
    ld a,d
    ld (QHEAD),a
    ld d,0
    ld hl,QBUF
    add hl,de
    ld (hl),b
.full:
    ld a,1
    ldh ($70),a
    pop de
    pop hl
    ret

; ---- replaces `ld a,($c82b) / bit 7,a / ret z` (first 3 bytes) at $3894: send the packet at hl (RAM-built palette sets)
H_SGB2:
    ldh a,($4f)
    cp $fe
    jr z,.cgb
    ld a,($c82b)
    bit 7,a
    ret z
    call $38a0
    jp $383f
.cgb:
    ld a,(hl)
    cp $51                  ; PAL_SET only
    ret nz
    ld c,5                  ; read 10 bytes under SVBK 1 onto the stack ...
.rd:
    ld a,(hl+)
    ld e,a
    ld a,(hl+)
    ld d,a
    push de
    dec c
    jr nz,.rd
    ld a,4
    ldh ($70),a
    ld hl,DPKT+8            ; ... and pop them into bank 4 (last pair first)
    ld c,5
.wr:
    pop de
    ld a,e
    ld (hl+),a
    ld a,d
    ld (hl),a
    dec hl
    dec hl
    dec hl
    dec c
    jr nz,.wr
    ld a,1
    ld (DPEND),a
    ld a,1
    ldh ($70),a
    ret

; ---- main loop hook: replaces `call $01a4` (state machine) -----------------------------------
H_MAIN:
    call $01a4
    ldh a,($4f)
    cp $fe
    ret nz
    ld a,4                  ; the whole colour pass runs with WRAM bank 4
    ldh ($70),a
    ldh a,($8c)
    ld (RB),a
.q:                         ; run queued SGB packets
    ld a,(QTAIL)
    ld b,a
    ld a,(QHEAD)
    cp b
    jr z,.qd
    ld e,b
    ld d,0
    ld hl,QBUF
    add hl,de
    ld a,(hl)
    push af
    ld a,b
    inc a
    and 7
    ld (QTAIL),a
    pop af
    call PROC
    jr .q
.qd:
    ld a,(DPEND)
    and a
    call nz,PROCDYN
    xor a
    ld (DONE),a
    ldh a,($47)
    ld hl,CBGP
    cp (hl)
    jr z,.s1
    ld (hl),a
    call FP
.s1:
    ldh a,($48)
    inc hl
    cp (hl)
    jr z,.s2
    ld (hl),a
    call FP
.s2:
    ldh a,($49)
    inc hl
    cp (hl)
    jr z,.s3
    ld (hl),a
    call FP
.s3:
    call OVRCALL1
    ld a,(FLG)
    bit 0,a
    jr z,.np
    res 0,a
    ld (FLG),a
    call OVRCALL2
    ld a,1
    ld (DONE),a
.np:                        ; map signature: scroll tiles + BG map select
    ld a,($c707)            ; SCY shadow
    srl a
    srl a
    srl a
    ld c,a
    ld a,(BGSP)             ; BG-space override map: the attribute shadow does not depend on the scroll
    and a
    jr z,.nbg
    ld a,(CSCY)
    ld c,a
.nbg:
    ld a,($c708)            ; SCX shadow
    srl a
    srl a
    srl a
    ld d,a
    ldh a,($40)
    and 8
    ld e,a
    ld a,(FLG)
    bit 1,a
    jr nz,.bm
    ld a,(CSCY)
    cp c
    jr nz,.bm
    ld a,(CSCX)
    cp d
    jr nz,.bm
    ld a,(CLC3)
    cp e
    jr z,.nb
.bm:
    ld a,c
    ld (CSCY),a
    ld a,d
    ld (CSCX),a
    ld a,e
    ld (CLC3),a
    ld a,(FLG)
    res 1,a
    ld (FLG),a
    call OBJPASS            ; the rebuild can outlast VBlank: colour the sprites before it
    call BUILD
.nb:
    call OBJPASS
    ld a,(DONE)
    ld c,a
    bit 0,c
    jr z,.a1
    ld a,(UPL)
    or 3
    ld (UPL),a
.a1:
    bit 1,c
    jr z,.a2
    ld a,(GDBN)
    ld (GDB),a
    xor a
    ld (GD),a
.a2:
    ld a,(RB)
    rst $10
    ld a,1                  ; back to WRAM bank 1 for the game
    ldh ($70),a
    ret

; apply the pending RAM-built PAL_SET packet (ids -> DPAL colours, optional ATF)
PROCDYN:
    xor a
    ld (DPEND),a
    ld a,$0d
    rst $10
    ld b,0
.pl:
    ld a,b
    add a,a
    ld e,a
    ld d,0
    ld hl,DPKT+1
    add hl,de
    ld a,(hl+)
    ld e,a
    ld d,(hl)               ; de = palette id
    ld hl,DPAL
.s:
    ld a,(hl+)
    ld c,a
    ld a,(hl+)
    cp $ff
    jr z,.nx2               ; unknown id: keep palette
    cp d
    jr nz,.nx
    ld a,c
    cp e
    jr z,.found
.nx:
    ld a,8
    add a,l
    ld l,a
    jr nc,.s
    inc h
    jr .s
.found:
    ld a,b
    add a,a
    add a,a
    add a,a
    ld e,a
    ld d,$d0
    ld c,8
.cc:
    ld a,(hl+)
    ld (de),a
    inc de
    dec c
    jr nz,.cc
    ld a,(FLG)
    or 1
    ld (FLG),a
.nx2:
    inc b
    ld a,b
    cp 4
    jr nz,.pl
    ld a,(DPKT+9)           ; ATF byte
    bit 7,a
    jr z,.dn
    and $3f
    ld e,a
    add a,a
    add a,e
    ld e,a
    ld d,0
    ld hl,ATFTAB
    add hl,de
    ld a,(hl+)
    and a
    jr z,.dn
    ld c,a                  ; ROM bank of the map
    ld e,(hl)
    inc hl
    ld d,(hl)
    ld a,c
    rst $10
    ld h,d
    ld l,e
    ld de,MAP
    ld bc,360
.ac:
    ld a,(hl+)
    ld (de),a
    inc de
    dec bc
    ld a,b
    or c
    jr nz,.ac
    ld a,(FLG)
    or 2
    ld (FLG),a
.dn:
    ld a,(RB)
    rst $10
    ret

FP:                         ; FLG |= 1
    ld a,(FLG)
    or 1
    ld (FLG),a
    ret

; ---- run one queued packet record: a = packet index -----------------------------------------
PROC:
    ld l,a
    ld h,0
    add hl,hl
    ld de,$6400             ; PKTTAB (bank $0d)
    add hl,de
    ld a,$0d
    rst $10
    ld a,(hl+)
    ld h,(hl)
    ld l,a
    or h
    jr z,.sk
    call RUNREC
.sk:
    ld a,(RB)
    rst $10
    ret

; hl = record (ROM bank $0d mapped, SVBK=4)
RUNREC:
.n:
    ld a,(hl+)
    and a
    ret z
    cp 1
    jr z,.pal
    cp 2
    jp z,.atf
    cp 3
    jp z,.rect
    cp 4
    jp z,.cells
    ret
.pal:
    ld de,PALS
    ld b,32
.pl:
    ld a,(hl+)
    ld (de),a
    inc de
    dec b
    jr nz,.pl
    ld a,(FLG)
    or 1
    ld (FLG),a
    jp .n
.atf:
    ld a,(hl+)              ; bank
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    push hl
    rst $10
    ld h,d
    ld l,e
    ld de,MAP
    ld bc,360
.ac:
    ld a,(hl+)
    ld (de),a
    inc de
    dec bc
    ld a,b
    or c
    jr nz,.ac
    ld a,$0d
    rst $10
    pop hl
    ld a,(FLG)
    or 2
    ld (FLG),a
    jp .n
.rect:
    ld a,(hl+)
    ld (T0),a               ; x1
    ld a,(hl+)
    ld (T1),a               ; y1
    ld a,(hl+)
    ld (T2),a               ; x2
    ld a,(hl+)
    ld (T3),a               ; y2
    ld a,(hl+)
    ld (T4),a               ; palette
    push hl
.ry:
    ld a,(T1)
    ld d,a
    ld a,(T0)
    ld e,a
    call MAPPTR
    ld a,(T2)
    ld b,a
    ld a,(T0)
    ld c,a
    ld a,b
    sub c
    inc a
    ld b,a                  ; count
    ld a,(T4)
.rx:
    ld (hl+),a
    dec b
    jr nz,.rx
    ld a,(T1)
    inc a
    ld (T1),a
    ld b,a
    ld a,(T3)
    cp b
    jr nc,.ry
    pop hl
    ld a,(FLG)
    or 2
    ld (FLG),a
    jp .n
.cells:
    ld a,(hl+)
    ld b,a
.cl:
    push bc
    ld a,(hl+)
    ld e,a
    ld a,(hl+)
    ld d,a
    ld a,(hl+)
    ld (T4),a
    push hl
    call MAPPTR
    ld a,(T4)
    ld (hl),a
    pop hl
    pop bc
    dec b
    jr nz,.cl
    ld a,(FLG)
    or 2
    ld (FLG),a
    jp .n

; d = y, e = x  ->  hl = MAP + y*20 + x   (SVBK 4)
MAPPTR:
    ld a,d
    add a,a
    ld hl,ROWOFF
    add a,l
    ld l,a
    ld a,(hl+)
    ld h,(hl)
    ld l,a
    ld d,0
    add hl,de
    ret

; a = DMG palette register, hl = dest (4 palettes x 4 colours); SVBK 4
; ---- stubs into the bank $0d override code -------------------------------------------------------
OVRCALL1:
    ld a,$0d
    rst $10
    call OVRSTEP
    ld a,(RB)
    rst $10
    ret
OVRCALL2:
    ld a,$0d
    rst $10
    call OVRDER
    ld a,(RB)
    rst $10
    ret

; rebuild the BG attribute shadow from MAP for the current scroll position (SVBK 4)
BUILD:
    xor a
    ld (T5),a               ; sy
.row:
    ld a,(CSCY)
    ld b,a
    ld a,(T5)
    add a,b
    and 31                  ; ty
    ld b,a
    ld a,(BGSP)
    and a
    jr z,.nb2
    ld a,(T5)               ; BG-space map: build all 32 rows, ty = row counter
    ld b,a
.nb2:
    ld a,b
    and 7
    swap a
    add a,a                 ; (ty&7)*32
    ld (T0),a               ; row base low
    ld a,b
    srl a
    srl a
    srl a
    add a,$d2
    ld (T1),a               ; row base high
    ld a,(BGSP)
    and a
    jr z,.scr
    ld l,b                  ; BG-space map: source row = BGMAP + ty*20
    ld h,0
    add hl,hl
    add hl,hl
    ld d,h
    ld e,l
    add hl,hl
    add hl,hl
    add hl,de
    ld de,BGMAP
    add hl,de
    ld d,h
    ld e,l
    jr .got
.scr:
    ld a,(T5)
    cp 18
    jr c,.sr
    ld a,17
.sr:
    add a,a
    ld hl,ROWOFF
    add a,l
    ld l,a
    ld a,(hl+)
    ld d,(hl)
    ld e,a                  ; de = source row (20 cells)
.got:
    ld a,(CSCX)
    and 31
    ld c,a                  ; col0
    ld a,(T0)
    or c
    ld l,a
    ld a,(T1)
    ld h,a                  ; hl = dest at col0
    ld a,32
    sub c
    cp 20
    jr c,.n1
    ld a,20
.n1:
    ld b,a                  ; n1 = min(20, 32-col0)
    ld a,20
    sub b
    ld (T2),a               ; n2 = cells after the wrap
.c1:
    ld a,(de)
    ld (hl+),a
    inc de
    dec b
    jr nz,.c1
    ld a,(T2)
    and a
    jr z,.nw
    ld b,a
    ld a,(T0)
    ld l,a                  ; wrap to column 0
.c2:
    ld a,(de)
    ld (hl+),a
    inc de
    dec b
    jr nz,.c2
.nw:
    dec de                  ; 21st column repeats the last cell
    ld a,(CSCX)
    add a,20
    and 31
    ld c,a
    ld a,(T0)
    or c
    ld l,a
    ld a,(T1)
    ld h,a
    ld a,(de)
    ld (hl),a
    ld a,(T5)
    inc a
    ld (T5),a
    ld c,19
    ld a,(BGSP)
    and a
    jr z,.nt
    ld c,32
.nt:
    ld a,(T5)
    cp c
    jp nz,.row
    ld a,(CLC3)
    and a
    ld a,$98
    jr z,.mb
    ld a,$9c
.mb:
    ld (GDBN),a
    ld a,(DONE)
    or 2
    ld (DONE),a
    ret

; ---- OBJ palette bits from the cell under each sprite (SVBK 4) ------------------------------
OBJPASS:
    ld hl,$c000
    ld b,40
    ld a,(OVRON)
    and a
    jr z,.o
.f:                         ; override active: palette purely by tile class (fast, finishes before VBlank)
    inc hl
    inc hl
    ld a,(hl+)
    ld e,a
    ld d,$d7
    ld a,(de)
    ld c,a
    ld a,(hl)
    and $f0
    ld d,a
    and $10
    swap a
    add a,a
    add a,a
    or c
    or d
    ld (hl+),a
    dec b
    jr nz,.f
    ret
.o:
    ld a,(hl+)              ; y
    and a
    jr z,.skip3
    cp 160
    jr nc,.skip3
    sub 12
    jr nc,.y1
    xor a
.y1:
    srl a
    srl a
    srl a
    cp 18
    jr c,.y2
    ld a,17
.y2:
    ld d,a
    ld a,(hl+)              ; x
    and a
    jr z,.skip2
    sub 4
    jr nc,.x1
    xor a
.x1:
    srl a
    srl a
    srl a
    cp 20
    jr c,.x2
    ld a,19
.x2:
    ld e,a
    inc hl                  ; tile
    push hl
    ld a,d
    add a,a
    ld hl,ROWOFF
    add a,l
    ld l,a
    ld a,(hl+)
    ld h,(hl)
    ld l,a
    ld d,0
    add hl,de
    ld c,(hl)               ; palette 0-3
    pop hl
    ld a,(DKHI)
    and a
    jr z,.nodk
    dec hl
    ld a,(hl+)              ; tile
    ld e,a
    ld a,(DKLO)
    ld d,a
    ld a,e
    cp d
    jr c,.nodk
    ld a,(DKHI)
    ld d,a
    ld a,e
    cp d
    jr nc,.nodk
    ld c,7                  ; Donkey Kong -> OBJ palette 7 (brown / beige)
.nodk:
    ld a,(hl)               ; flags
    and $f0
    ld d,a
    and $10
    swap a
    add a,a
    add a,a                 ; 4 when OBP1
    or c
    or d
    ld (hl+),a
    dec b
    jr nz,.o
    ret
.skip3:
    inc hl
.skip2:
    inc hl
    inc hl
    dec b
    jr nz,.o
    ret

; ---- VBlank hook: replaces `call $16e2` ---------------------------------------------------------
H_DMA:                      ; replaces the VBlank OAM-DMA call: colour the sprites first (the game rewrites OAM after the main-loop pass)
    ld a,4
    ldh ($70),a
    call OBJPASS
    ld a,1
    ldh ($70),a
    jp $ff80
H_VB2:
    call $16e2
    ldh a,($4f)
    cp $fe
    ret nz
    ld a,4
    ldh ($70),a
    call VB2W
    ld a,1
    ldh ($70),a
    ret
VB2W:
    ld a,(UPL)
    and a
    jr z,.gd
    bit 0,a
    jr z,.obj
    res 0,a
    ld (UPL),a
    ld a,$80
    ldh ($68),a
    ld hl,CPAL
    ld c,$69
    ld b,64
.lb:
    ld a,(hl+)
    ld ($ff00+c),a
    dec b
    jr nz,.lb
    jr .done
.obj:
    res 1,a
    ld (UPL),a
    ld a,$80
    ldh ($6a),a
    ld hl,CPAL+64
    ld c,$6b
    ld b,64
.lo:
    ld a,(hl+)
    ld ($ff00+c),a
    dec b
    jr nz,.lo
.done:
    ret
.gd:
    ld a,(GD)
    cp 4
    jr c,.gs
    ld a,(HUDW)
    and a
    ret z
    dec a
    ld c,a
    xor a
    ld (HUDW),a
    ld a,1
    ldh ($4f),a
    ld hl,$9c00
    ld b,20
.hw:
    ld a,c
    ld (hl+),a
    dec b
    jr nz,.hw
    xor a
    ldh ($4f),a
    ret
.gs:
    ld c,a                  ; slice
    ld a,(GDB)
    cp $98
    ret c
    sub $80
    add a,c
    ld b,a                  ; destination high byte within VRAM
    ld a,c
    inc a
    ld (GD),a
    cp 4
    jr c,.gk
    ld a,$ff
    ld (GD),a
.gk:
    ld a,1
    ldh ($4f),a
    ld a,$d2
    add a,c
    ldh ($51),a             ; source high ($d2xx..$d5xx)
    xor a
    ldh ($52),a
    ld a,b
    ldh ($53),a
    xor a
    ldh ($54),a
    ld a,$0f
    ldh ($55),a             ; general DMA, 16 blocks = 256 bytes
    xor a
    ldh ($4f),a
    ret

; row offsets of the 20-wide attribute map
.org $3fa0
ROWOFF:
    dw MAP,MAP+20,MAP+40,MAP+60,MAP+80,MAP+100,MAP+120,MAP+140,MAP+160
    dw MAP+180,MAP+200,MAP+220,MAP+240,MAP+260,MAP+280,MAP+300,MAP+320,MAP+340

; ======================================================================================================
; bank $0d: stage colour override for Big-City 1-2 (data blob OVRD: 8 BG pal, 8 OBJ pal, 20x18 map, 256 tile classes)
; bank-0 helper: read hl..hl+bc from ROM bank a into de, then map bank $0d again (rst $10 = set ROM bank)
.org $3fc4
COPYBK:
    rst $10
.l:
    ld a,(hl+)
    ld (de),a
    inc de
    dec bc
    ld a,b
    or c
    jr nz,.l
    ld a,$0d
    rst $10
    ret

.bank 0x0d
.org $7c00
OVRSTEP:                    ; per frame, after packets were handled (SVBK 4, bank $0d mapped)
    call DKSET
    ldh a,($8f)             ; the in-stage scan-line chain is running (it idles at 0 on the map scenes)
    and a
    jp z,.off
    ld a,($c85d)
    and a
    jp nz,.th
    ld a,($c85c)            ; level-sequence step: 16 = Big-City 1-2, 19 = the scrolling city stage
    ld b,0
    cp 16
    jr z,.chk
    inc b
    cp 19
    jr z,.chk
    inc b                   ; stage 2: forest 2-1 (steps 41 = intro, 42 = play)
    cp 41
    jr z,.chk
    cp 42
    jr z,.chk
    jp .th
.chk:
    ld a,(OVRON)
    and a
    jp z,.apply
    ld a,(OVRSEL)
    cp b
    jp nz,.apply
    ld a,(FLG)
    and 3
    ret z
.th:                        ; every other play stage: its world's colour theme (selector 3)
    ld a,($c85d)
    ld d,a
    ld a,($c85c)
    ld e,a
    ld c,0
    ld hl,THSTEP
.tw:
    ld a,(hl+)
    ld b,a
    ld a,(hl+)
    cp $ff
    jr z,.tdone
    cp d
    jr c,.tinc
    jr nz,.tdone
    ld a,e
    cp b
    jr c,.tdone
.tinc:
    inc c
    jr .tw
.tdone:
    ld a,c
    and a
    jp z,.off
    ld b,3
    ld a,(OVRON)
    and a
    jr z,.tap
    ld a,(OVRSEL)
    cp 3
    jr nz,.tap
    ld a,(OVRTH)
    cp c
    jr nz,.tap
    ld a,(FLG)
    and 3
    ret z
.tap:
    ld a,c
    ld (OVRTH),a
    jp .apply
.st4:                       ; world theme: palettes by world, empty map, shared character classes
    xor a
    ld (BGSP),a
    ld a,(OVRTH)
    dec a
    srl a
    ld h,a
    ld a,0
    rra
    ld l,a
    ld de,OVRDT
    add hl,de
    ld a,$0c
    ld de,OVRP
    ld bc,128
    call COPYBK
    ld hl,MAP
    ld bc,360
.z:
    xor a
    ld (hl+),a
    dec bc
    ld a,b
    or c
    jr nz,.z
    ld a,$0c
    ld hl,OVRDG
    ld de,OBJCLS
    ld bc,256
    call COPYBK
    jp .on

.apply:
    ld a,b
    ld (OVRSEL),a
    ld hl,OVRD
    ld de,OVRP
    ld bc,128
    call COPY               ; palettes (shared by both stages)
    ld a,(OVRSEL)
    cp 2
    jp z,.st3
    cp 3
    jp z,.st4
    and a
    jr nz,.st2
    xor a
    ld (BGSP),a
    ld de,MAP
    ld bc,360
    call COPY
    jr .cls
.st2:
    ld a,1
    ld (BGSP),a
    ld a,$0c                ; stage 2 data lives in bank $0c: copy through the bank-0 helper
    ld hl,OVRD2
    ld de,BGMAP
    ld bc,640
    call COPYBK
    ld a,$0c
    ld de,OBJCLS
    ld bc,256
    call COPYBK
    jr .on
.cls:
    ld de,OBJCLS
    ld bc,256
    call COPY
    jr .on
.st3:                       ; forest 2-1: own palettes + map + classes, all from bank $0c
    xor a
    ld (BGSP),a
    ld a,$0c
    ld hl,OVRDC
    ld de,OVRP
    ld bc,128
    call COPYBK
    ld a,$0c
    ld de,MAP
    ld bc,360
    call COPYBK
    ld a,$0c
    ld de,OBJCLS
    ld bc,256
    call COPYBK
.on:
    ld a,1
    ld (OVRON),a
    ld a,8
    ld (HUDW),a
    ld a,(FLG)
    or 3
    ld (FLG),a
    ret
.off:
    ld a,(OVRON)
    and a
    ret z
    xor a
    ld (OVRON),a
    ld (BGSP),a
    inc a
    ld (HUDW),a
    ld a,(FLG)
    or 3
    ld (FLG),a
    ret
COPY:
    ld a,(hl+)
    ld (de),a
    inc de
    dec bc
    ld a,b
    or c
    jr nz,COPY
    ret
OVRDER:                     ; derive CPAL from BGP / OBP0 / OBP1
    ld a,(OVRON)
    and a
    jr nz,.ov
    xor a
    ld (T6),a
    ld a,$d0
    ld (T7),a
    ld a,4
    ld (T8),a
    ld hl,CPAL
    ld a,(CBGP)
    call DERIVE
    ld hl,CPAL+64
    ld a,(CBGP+1)
    call DERIVE
    ld hl,CPAL+96
    ld a,(CBGP+2)
    call DERIVE
    jp DKPAL
.ov:
    xor a
    ld (T6),a
    ld a,$d6
    ld (T7),a
    ld a,8
    ld (T8),a
    ld hl,CPAL
    ld a,(CBGP)
    call DERIVE
    ld a,4
    ld (T8),a
    ld a,$40
    ld (T6),a
    ld hl,CPAL+64
    ld a,(CBGP+1)
    call DERIVE
    ld a,$60
    ld (T6),a
    ld hl,CPAL+96
    ld a,(CBGP+2)
    call DERIVE
    jp DKPAL
DERIVE:                     ; a = register (shade per colour), hl = dest, (T7:T6) = palettes base, T8 = count
    ld (T0),a
    xor a
    ld (T1),a
.pl:
    ld a,(T0)
    ld c,a
    ld b,4
.cl:
    ld a,c
    and 3
    add a,a
    ld e,a
    ld a,(T1)
    add a,a
    add a,a
    add a,a
    add a,e
    ld e,a
    ld a,(T6)
    add a,e
    ld e,a
    ld a,(T7)
    ld d,a
    ld a,(de)
    ld (hl+),a
    inc de
    ld a,(de)
    ld (hl+),a
    srl c
    srl c
    dec b
    jr nz,.cl
    ld a,(T1)
    inc a
    ld (T1),a
    ld b,a
    ld a,(T8)
    cp b
    jr nz,.pl
    ret

DKSET:                      ; set DKLO/DKHI for the Donkey Kong stages (in-stage only), flag a palette refresh on change
    ldh a,($8f)
    and a
    jr z,.no
    ld a,($c85c)
    ld c,a
    ld hl,DKTAB
.l:
    ld a,(hl+)
    cp $ff
    jr z,.no
    cp c
    jr z,.f
    inc hl
    inc hl
    jr .l
.f:
    ld a,(hl+)
    ld (DKLO),a
    ld a,(hl)
    jr .set
.no:
    xor a
.set:
    ld (DKHI),a
    ld hl,DKP
    cp (hl)
    ret z
    ld (hl),a
    ld a,(FLG)
    or 1
    ld (FLG),a
    ret
DKPAL:                      ; overwrite OBJ palette 7 with the Donkey Kong colours
    ld a,(DKHI)
    and a
    ret z
    ld hl,DKPALD
    ld de,CPAL+120
    ld bc,8
    jp COPY
DKTAB:                      ; step, first tile, last tile+1  (steps are the level-sequence counter $c85c)
    db 2,$90,$9e
    db 5,$50,$5e
    db 8,$80,$b2
    db 11,$90,$9e
    db $ff
DKPALD:                     ; BGR555: transparent, dark brown, brown, beige
    dw $0000,$0887,$1d33,$4ede
THSTEP: dw 13,39,78,104,143,182,208,247,299,$ffff
