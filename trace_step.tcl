# Stepper aislado de IA de enemigos:
#  1) entra a gameplay (3 espacios) -> estado correcto
#  2) force-load la sala (tr_room.txt decimal) via sub_64DD -> puebla BAT+mapa
#  3) llama sub_438D (dispatcher de enemigos) N veces con retorno-trampa en 0xC000,
#     volcando la tabla BAT en cada paso. Incrementa 0xEAC9 (frame ctr) por paso.
set throttle off
set fp [open "tr_room.txt" r]; set ::room [string trim [read $fp]]; close $fp
set ::fc 0; set ::log {}
proc dumpbat {} {
    set rm [debug read memory 0xe320]
    set line "$::fc R[format %02X $rm]"
    for {set s 0} {$s < 8} {incr s} {
        set base [expr {0xe416 + $s*5}]
        set tp [debug read memory [expr {$base+1}]]
        if {$tp != 0} {
            append line " | t[format %02X $tp] r[debug read memory [expr {$base+2}]] c[debug read memory [expr {$base+3}]] f[debug read memory [expr {$base+4}]] p[debug read memory $base]"
        }
    }
    lappend ::log $line
}
proc callstep {} {
    # preparar llamada a sub_438D con retorno a 0xC000
    set sp [expr {[reg SP]-2}]
    debug write memory $sp 0x00
    debug write memory [expr {$sp+1}] 0xC0
    reg SP $sp
    reg PC 0x438D
}
proc ontrap {} {
    dumpbat
    # avanzar contador de frame global (para animaciones tipo 0x36)
    debug write memory 0xeac9 [expr {([debug read memory 0xeac9]+1) & 0xff}]
    incr ::fc
    if {$::fc >= 240} {
        debug remove_bp $::trap
        set f [open "step_out.txt" w]; puts $f "ROOM $::room"; puts $f [join $::log "\n"]; close $f
        set f [open "step_done.txt" w]; puts $f done; close $f
        exit
    }
    callstep
}
proc start {} {
    # force-load la sala
    debug write memory 0xe320 $::room
    set sp [expr {[reg SP]-2}]; debug write memory $sp 0x70; debug write memory [expr {$sp+1}] 0x40
    reg SP $sp; reg PC 0x64DD
    debug set_bp 0x4070 {} { afterload }
}
proc afterload {} {
    debug remove_bp 0x4070
    set ::trap [debug set_bp 0xC000 {} { ontrap }]
    callstep
}
after time 6 "keymatrixdown 8 1"; after time 6.3 "keymatrixup 8 1"
after time 8 "keymatrixdown 8 1"; after time 8.3 "keymatrixup 8 1"
after time 10 "keymatrixdown 8 1"; after time 10.3 "keymatrixup 8 1"
after time 13 { start }
