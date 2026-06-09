# Halla el contador de llaves: entra a gameplay (0x70), localiza la llave en COLL
# (0xE386), y BARRE la posicion del jugador alrededor de ella hasta que la tabla
# COLL se limpia (=recogida). Entonces vuelca la RAM antes/despues.
set throttle off
set ::fc 0
set ::kx -1; set ::ky -1; set ::slot -1
set ::picked 0
proc dumpram {tag} {
    set f [open "kp_$tag.bin" w]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block memory 0xe000 0x900]; close $f
}
proc sample {} {
    if {$::fc == 0} {
        for {set s 0} {$s < 16} {incr s} {
            set b [expr {0xe386 + $s*5}]
            set tp [debug read memory [expr {$b+1}]]
            if {$tp >= 0x30 && $tp <= 0x35 && $tp != 0x34} {
                set ::slot $b
                set ::kx [debug read memory [expr {$b+2}]]   ;# field "row" = X
                set ::ky [debug read memory [expr {$b+3}]]   ;# field "col" = Y
                set f [open kp_info.txt w]; puts $f "key type=[format %02X $tp] X(b2)=$::kx Y(b3)=$::ky"; close $f
                break
            }
        }
        dumpram before
    }
    # barrer la posicion del jugador alrededor de la llave (X a 0xE334, Y a 0xE335)
    if {$::slot >= 0 && !$::picked} {
        set dx [expr {($::fc % 5) - 1}]   ;# -1..3
        debug write memory 0xe334 [expr {$::kx + $dx}]
        debug write memory 0xe335 $::ky
        # detectar recogida: el type del slot se hizo 0
        set nowtype [debug read memory [expr {$::slot+1}]]
        if {$nowtype == 0} { set ::picked $::fc; dumpram after; set f [open kp_picked.txt w]; puts $f "picked at fc=$::fc dx=$dx"; close $f }
    }
    if {$::fc >= 60} {
        if {!$::picked} { dumpram after; set f [open kp_picked.txt w]; puts $f "NO PICKUP"; close $f }
        set f [open kp_done.txt w]; puts $f done; close $f
        debug remove_bp $::bp; exit
    }
    incr ::fc
}
after time 6 "keymatrixdown 8 1"; after time 6.3 "keymatrixup 8 1"
after time 8 "keymatrixdown 8 1"; after time 8.3 "keymatrixup 8 1"
after time 10 "keymatrixdown 8 1"; after time 10.3 "keymatrixup 8 1"
after time 13 { set ::bp [debug set_bp 0x4070 {} { sample }] }
