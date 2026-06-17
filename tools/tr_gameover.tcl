# Oraculo de GAME OVER (sub_4F16): warp al juego, dejar correr unos frames
# para poblar el HUD, forzar vidas=0 y saltar a sub_4F16; al llegar al RET
# (0x4F3F) volcar la name table (filas 13-15, donde se dibuja "GAME OVER").
# Salida -> tr_gameover_out.txt (3 lineas hex de 32 bytes c/u).
set throttle off
set ::state 0
set ::frames 0
proc dumprows {} {
    set out {}
    foreach row {13 14 15} {
        set line ""
        for {set c 0} {$c < 32} {incr c} {
            append line [format %02X [debug read VRAM [expr {0x1800 + $row*32 + $c}]]]
        }
        lappend out $line
    }
    set f [open "tr_gameover_out.txt" w]; puts $f [join $out "\n"]; close $f
    set f [open "tr_gameover_done.txt" w]; puts $f done; close $f
    exit
}
proc atloop {} {
    incr ::frames
    if {$::frames >= 30} {
        # forzar game over: 0 vidas, saltar a sub_4F16
        debug remove_bp $::bploop
        debug write memory 0xe324 0
        set ::bpend [debug set_bp 0x4F3F {} { dumprows }]
        reg PC 0x4F16
    }
}
proc forceload {} {
    debug write memory 0xe320 0x70
    debug write memory 0xe322 14
    debug write memory 0xe323 11
    debug write memory 0xe334 14
    debug write memory 0xe335 11
    set sp [expr {[reg SP] - 2}]
    debug write memory $sp 0x70
    debug write memory [expr {$sp + 1}] 0x40
    reg SP $sp
    reg PC 0x64DD
    set ::bploop [debug set_bp 0x4070 {} { atloop }]
}
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 16 { forceload }
after time 120 {
    set f [open "tr_gameover_err.txt" w]; puts $f "timeout state=$::state f=$::frames"; close $f
    exit
}
