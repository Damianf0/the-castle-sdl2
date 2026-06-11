# Varios saltos desde reposo, distinta tecla y duracion, para ver la trayectoria.
# Loga Y,X,pat por frame. Teclas: SPACE=row8 m1, UP=row8 m32, DER=row8 m128.
set throttle off
set ::fc 0
set ::log {}
proc ev {f act} { if {$::fc == $f} { eval $act } }
proc sample {} {
    set y [debug read VRAM 0x1b20]; set x [debug read VRAM 0x1b21]; set p [debug read VRAM 0x1b22]
    lappend ::log "$::fc $y $x $p"
    # A: UP tap
    ev 20 {keymatrixdown 8 32}; ev 22 {keymatrixup 8 32}
    # B: UP hold largo
    ev 60 {keymatrixdown 8 32}; ev 85 {keymatrixup 8 32}
    # C: SPACE hold largo
    ev 120 {keymatrixdown 8 1}; ev 145 {keymatrixup 8 1}
    # D: UP+DER (salto con carrera)
    ev 180 {keymatrixdown 8 160}; ev 200 {keymatrixup 8 160}
    if {$::fc >= 230} {
        debug remove_bp $::bpid
        set f [open "jump_trace2.txt" w]; puts $f [join $::log "\n"]; close $f
        set f [open "tj2_done.txt" w]; puts $f done; close $f
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
