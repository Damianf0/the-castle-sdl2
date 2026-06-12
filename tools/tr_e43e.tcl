# Oraculo de ESTRUCTURALES e43e: lee "room_decimal pcol prow frames [holddir
# holdfrom]" de tr_e43e_in.txt, arranca el juego, force-carga la sala y traza
# por frame los 16 slots e43e: tipo,col,fila,f3,estado. Sin hold: jugador
# FIJADO (invulnerable). Con hold R/L: el jugador queda libre y desde el
# frame holdfrom mantiene esa direccion (escenarios de EMPUJE de la 0x34).
# Salida -> tr_e43e_out.txt (mismo formato que CASTLE_E43TRACE del port).
set throttle off
set fp [open "tr_e43e_in.txt" r]
set ::hold ""
set ::holdfrom 0
lassign [string trim [read $fp]] ::room ::pcol ::prow ::nframes ::hold ::holdfrom
close $fp
set ::fc -1
set ::log {}
proc sample {} {
    if {$::hold eq ""} {
        debug write memory 0xe334 $::pcol
        debug write memory 0xe335 $::prow
    } elseif {$::fc == $::holdfrom} {
        keymatrixdown 8 [expr {$::hold eq "L" ? 16 : 128}]
    }
    debug write memory 0xeae0 0
    debug write memory 0xe343 1
    if {$::fc == -1} { set ::fc 0 }
    set line "$::fc"
    for {set s 0} {$s < 16} {incr s} {
        set base [expr {0xe43e + $s*5}]
        set t [debug read memory $base]
        if {$t != 0} {
            append line " $s:[format %02X $t],[debug read memory [expr {$base+1}]],[debug read memory [expr {$base+2}]],[debug read memory [expr {$base+3}]]/s[format %02X [debug read memory [expr {$base+4}]]]"
        }
    }
    lappend ::log $line
    if {$::fc >= $::nframes} {
        debug remove_bp $::bpid
        set f [open "tr_e43e_out.txt" w]; puts $f [join $::log "\n"]; close $f
        set f [open "tr_e43e_done.txt" w]; puts $f done; close $f
        exit
    }
    incr ::fc
}
proc forceload {} {
    debug write memory 0xe320 $::room
    # punto de entrada: el loader respawnea al jugador desde E322/E323
    debug write memory 0xe322 $::pcol
    debug write memory 0xe323 $::prow
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
        set f [open "tr_e43e_err.txt" w]; puts $f $err; close $f
        exit
    }
}
after time 300 {
    set f [open "tr_e43e_err.txt" w]; puts $f "timeout fc=$::fc"; close $f
    exit
}
