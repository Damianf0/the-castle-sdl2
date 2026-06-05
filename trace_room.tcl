# Force-carga la sala (tr_room.txt, decimal), FIJA al jugador en el centro cada
# frame (para que el demo no navegue y cambie de sala) y traza la tabla BAT
# (0xE416, 8x5) ~200 frames. Salida -> tr_out.txt
set throttle off
set fp [open "tr_room.txt" r]; set ::room [string trim [read $fp]]; close $fp
set ::fc -1
set ::log {}
proc sample {} {
    # Fijar jugador al centro para evitar transicion de sala (demo mode)
    debug write memory 0xe334 14   ;# player col
    debug write memory 0xe335 12   ;# player row
    if {$::fc == -1} { set ::fc 0 }
    set rm [debug read memory 0xe320]
    set line "$::fc R[format %02X $rm] P[debug read memory 0xe334],[debug read memory 0xe335]"
    for {set s 0} {$s < 8} {incr s} {
        set base [expr {0xe416 + $s*5}]
        set tp [debug read memory [expr {$base+1}]]
        if {$tp != 0} {
            append line " | s$s t[format %02X $tp] r[debug read memory [expr {$base+2}]] c[debug read memory [expr {$base+3}]] f[debug read memory [expr {$base+4}]]"
        }
    }
    lappend ::log $line
    if {$::fc >= 200} {
        debug remove_bp $::bpid
        set f [open "tr_out.txt" w]; puts $f "ROOM $::room"; puts $f [join $::log "\n"]; close $f
        set f [open "tr_done.txt" w]; puts $f done; close $f
        exit
    }
    incr ::fc
}
proc forceload {} {
    debug write memory 0xe320 $::room
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
after time 13 { forceload }
