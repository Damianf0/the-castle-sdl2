# Medicion de velocidad del game loop real: en cada paso por 0x4070 registra
# "tiempo_emulado eaca". CTRL baja a los 60s, GRAPH a los 120s, fin a los 180s.
# Salida -> tr_speed_out.txt (analizar con tools/gen_speed.py o a mano).
set throttle off
set ::log {}

proc sample {} {
    debug write memory 0xe334 14
    debug write memory 0xe335 11
    debug write memory 0xeae0 0
    debug write memory 0xe343 1
    lappend ::log "[format %.5f [machine_info time]] [format %02X [debug read memory 0xeaca]]"
}

proc finish {why} {
    set f [open "tr_speed_out.txt" w]
    puts $f "# $why lines=[llength $::log]"
    puts $f [join $::log "\n"]
    close $f
    set f [open "tr_speed_done.txt" w]; puts $f done; close $f
    exit
}

proc forceload {} {
    debug write memory 0xe320 0x01
    debug write memory 0xe334 14
    debug write memory 0xe335 11
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
        set f [open "tr_speed_err.txt" w]; puts $f $err; close $f
        exit
    }
}
after time 60  "keymatrixdown 6 2"
after time 120 "keymatrixdown 6 4"
after time 180 { finish ok }
