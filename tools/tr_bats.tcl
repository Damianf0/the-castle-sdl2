# Oraculo BAT generico: lee "room_decimal pcol prow frames" de tr_bats_in.txt,
# arranca el juego, force-carga la sala, FIJA al jugador (invulnerable) y traza
# por frame los 8 slots BAT (0xE416): col,fila,flags. Salida -> tr_bats_out.txt
set throttle off
set fp [open "tr_bats_in.txt" r]
lassign [string trim [read $fp]] ::room ::pcol ::prow ::nframes
close $fp
set ::fc -1
set ::log {}
proc sample {} {
    debug write memory 0xe334 $::pcol
    debug write memory 0xe335 $::prow
    debug write memory 0xeae0 0
    debug write memory 0xe343 1
    if {$::fc == -1} { set ::fc 0 }
    set line "$::fc"
    for {set s 0} {$s < 8} {incr s} {
        set base [expr {0xe416 + $s*5}]
        if {[debug read memory $base] != 0} {
            append line " $s:[debug read memory [expr {$base+2}]],[debug read memory [expr {$base+3}]]/f[format %02X [debug read memory [expr {$base+4}]]]"
        }
    }
    lappend ::log $line
    if {$::fc >= $::nframes} {
        debug remove_bp $::bpid
        set f [open "tr_bats_out.txt" w]; puts $f [join $::log "\n"]; close $f
        set f [open "tr_bats_done.txt" w]; puts $f done; close $f
        exit
    }
    incr ::fc
}
proc forceload {} {
    debug write memory 0xe320 $::room
    debug write memory 0xe334 $::pcol
    debug write memory 0xe335 $::prow
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
        set f [open "tr_bats_err.txt" w]; puts $f $err; close $f
        exit
    }
}
after time 300 {
    set f [open "tr_bats_err.txt" w]; puts $f "timeout fc=$::fc"; close $f
    exit
}
