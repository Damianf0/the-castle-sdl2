# Oraculo de PICKUP: lee "room_decimal pcol prow frames holddir holdfrom" de
# tr_pick_in.txt; el jugador spawnea en (pcol,prow) via E322/E323, queda
# LIBRE (invulnerable) y desde holdfrom mantiene la direccion para caminar
# sobre los items. Traza por frame: slots e3d6 activos (tipo,col,fila) +
# score (E33D-3F) + llaves (E337-3C) + vidas (E336).
# Salida -> tr_pick_out.txt (mismo formato que CASTLE_PICKTRACE del port).
set throttle off
set fp [open "tr_pick_in.txt" r]
set ::givemap 0
lassign [string trim [read $fp]] ::room ::pcol ::prow ::nframes ::hold ::holdfrom ::givemap
close $fp
set ::fc -1
set ::log {}
proc sample {} {
    if {$::fc == -1} { set ::fc 0 }
    if {$::fc == $::holdfrom} {
        keymatrixdown 8 [expr {$::hold eq "L" ? 16 : 128}]
    }
    debug write memory 0xeae0 0
    debug write memory 0xe343 1
    set line "$::fc"
    for {set s 0} {$s < 16} {incr s} {
        set base [expr {0xe3d6 + $s*4}]
        if {[debug read memory $base] != 0} {
            append line " $s:[format %02X [debug read memory [expr {$base+1}]]],[debug read memory [expr {$base+2}]],[debug read memory [expr {$base+3}]]"
        }
    }
    append line " sc=[format %02X%02X%02X [debug read memory 0xe33d] [debug read memory 0xe33e] [debug read memory 0xe33f]]"
    append line " k=[debug read memory 0xe337],[debug read memory 0xe338],[debug read memory 0xe339],[debug read memory 0xe33a],[debug read memory 0xe33b],[debug read memory 0xe33c]"
    append line " v=[debug read memory 0xe336]"
    lappend ::log $line
    if {$::fc >= $::nframes} {
        debug remove_bp $::bpid
        set f [open "tr_pick_out.txt" w]; puts $f [join $::log "\n"]; close $f
        # dump gemelo de CASTLE_VRAMDUMP: name filas 0-4 + color chars 0-0x3F
        set v ""
        for {set a 0} {$a < 0xA0} {incr a} {
            append v [format %02X [debug read VRAM [expr {0x1800 + $a}]]]
        }
        append v "\n"
        for {set a 0} {$a < 0x200} {incr a} {
            append v [format %02X [debug read VRAM [expr {0x2000 + $a}]]]
        }
        set f [open "tr_pick_vram.txt" w]; puts $f $v; close $f
        set f [open "tr_pick_done.txt" w]; puts $f done; close $f
        exit
    }
    incr ::fc
}
proc forceload {} {
    debug write memory 0xe320 $::room
    debug write memory 0xe322 $::pcol
    debug write memory 0xe323 $::prow
    debug write memory 0xe334 $::pcol
    debug write memory 0xe335 $::prow
    if {$::givemap ne "" && $::givemap} {
        debug write memory 0xe321 [expr {[debug read memory 0xe321] | 8}]
    }
    set sp [expr {[reg SP] - 2}]
    debug write memory $sp 0x70
    debug write memory [expr {$sp + 1}] 0x40
    reg SP $sp
    reg PC 0x64DD
    set ::bpid [debug set_bp 0x4070 {} { sample }]
}
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 16 {
    if {[catch {forceload} err]} {
        set f [open "tr_pick_err.txt" w]; puts $f $err; close $f
        exit
    }
}
after time 300 {
    set f [open "tr_pick_err.txt" w]; puts $f "timeout fc=$::fc"; close $f
    exit
}
