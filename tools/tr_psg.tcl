# Oraculo PSG: arranca el juego, fija al jugador (sala 01) y registra TODAS
# las escrituras a los puertos PSG (0xA0 selector, 0xA1 dato) durante 12s
# emulados de juego. Salida: tr_psg_out.txt con lineas "tiempo reg valor".
set throttle off
set ::log {}
set ::sel 0

proc fixplayer {} {
    debug write memory 0xe334 14
    debug write memory 0xe335 11
    debug write memory 0xeae0 0
    debug write memory 0xe343 1
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
    debug set_bp 0x4070 {} { fixplayer }
    debug set_watchpoint write_io 0xa0 {} { set ::sel $::wp_last_value }
    debug set_watchpoint write_io 0xa1 {} {
        lappend ::log "[format %.5f [machine_info time]] $::sel $::wp_last_value"
    }
    after time 12 { finish }
}

proc finish {} {
    set f [open "tr_psg_out.txt" w]
    puts $f [join $::log "\n"]
    close $f
    set f [open "tr_psg_done.txt" w]; puts $f done; close $f
    exit
}

after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 16 {
    if {[catch {forceload} err]} {
        set f [open "tr_psg_err.txt" w]; puts $f $err; close $f
        exit
    }
}
after time 120 {
    set f [open "tr_psg_err.txt" w]; puts $f "timeout"; close $f
    exit
}
