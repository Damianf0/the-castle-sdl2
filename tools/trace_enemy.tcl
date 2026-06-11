# Traza la tabla BAT de enemigos (0xE416, 8 slots x 5: param,type,row,col,flags)
# frame a frame en gameplay, para ver el movimiento real de cada enemigo.
# Tambien la pos del jugador (col/row en 0xE334/0xE335) por si los enemigos
# reaccionan a el. Loop principal = 0x4070 (1x/frame).
set throttle off
set ::fc 0
set ::log {}
proc sample {} {
    set line "$::fc"
    # jugador col,row
    append line " P[debug read memory 0xe334],[debug read memory 0xe335]"
    # 8 slots BAT
    for {set s 0} {$s < 8} {incr s} {
        set base [expr {0xe416 + $s*5}]
        set tp [debug read memory [expr {$base+1}]]
        if {$tp != 0} {
            set pm [debug read memory $base]
            set rw [debug read memory [expr {$base+2}]]
            set cl [debug read memory [expr {$base+3}]]
            set fl [debug read memory [expr {$base+4}]]
            append line " | s$s t[format %02X $tp] r$rw c$cl f$fl p$pm"
        }
    }
    lappend ::log $line
    # dejar correr sin tocar nada (~250 frames) para ver el patron autonomo
    if {$::fc >= 250} {
        debug remove_bp $::bpid
        set f [open "enemy_trace.txt" w]; puts $f [join $::log "\n"]; close $f
        set f [open "te_done.txt" w]; puts $f done; close $f
        exit
    }
    incr ::fc
}
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 13 { set ::bpid [debug set_bp 0x4070 {} { sample }] }
