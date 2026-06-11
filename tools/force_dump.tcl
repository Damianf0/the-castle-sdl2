# openMSX: fuerza la carga de cada sala (sub_64DD @0x64DD) y vuelca las tablas
# de objetos reales (0xE380..0xE4A0). Usa un bp en el loop principal (0x4070).
set throttle off
set ::rooms {}
for {set hi 0} {$hi <= 9} {incr hi} {
    for {set lo 0} {$lo <= 9} {incr lo} {
        lappend ::rooms [expr {$hi*16 + $lo}]
    }
}
set ::idx -1

proc dumptables {rx} {
    set f [open [format "tests/fixtures/objs/objs_%02X.bin" $rx] w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block memory 0xE380 0x120]
    close $f
}

proc step {} {
    if {$::idx >= 0} {
        set rx [debug read memory 0xE320]
        dumptables $rx
    }
    incr ::idx
    if {$::idx >= [llength $::rooms]} {
        debug remove_bp $::bpid
        set f [open "force_done.txt" w]; puts $f "done [llength $::rooms]"; close $f
        exit
    }
    set r [lindex $::rooms $::idx]
    debug write memory 0xE320 $r
    set sp [expr {[reg SP] - 2}]
    debug write memory $sp 0x70
    debug write memory [expr {$sp + 1}] 0x40
    reg SP $sp
    reg PC 0x64DD
}

# entrar al juego primero, despues activar el bp que dispara la secuencia
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 11 {
    set ::bpid [debug set_bp 0x4070 {} { step }]
}
