# Traza la posicion del jugador (sprite slot 8: Y=0x1B20, X=0x1B21) frame a frame
# mientras camina y salta, para capturar la trayectoria REAL del movimiento.
# Prueba SPACE como salto (row8 mask 1). El loop principal es 0x4070 (1x/frame).
set throttle off
set ::fc 0
set ::log {}

proc sample {} {
    set y [debug read VRAM 0x1b20]
    set x [debug read VRAM 0x1b21]
    set p [debug read VRAM 0x1b22]
    lappend ::log "$::fc $y $x $p"
    # guion de inputs por frame
    if {$::fc == 10} { keymatrixdown 8 128 }   ;# derecha ON
    if {$::fc == 40} { keymatrixup   8 128 }   ;# derecha OFF
    if {$::fc == 50} { keymatrixdown 8 1 }     ;# SPACE (salto?) ON
    if {$::fc == 52} { keymatrixup   8 1 }     ;# SPACE OFF (tap)
    if {$::fc >= 110} {
        debug remove_bp $::bpid
        set f [open "jump_trace.txt" w]
        puts $f [join $::log "\n"]
        close $f
        set f [open "tj_done.txt" w]; puts $f done; close $f
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
