# Entra a gameplay, mueve al jugador (derecha luego izquierda) y vuelca la
# sprite attr table + pattern table en varios instantes para capturar los
# frames de animacion y el facing.
set throttle off
set ::n 0
proc snap {} {
    set t [format "%02d" $::n]
    set f [open "wattr_$t.bin" w]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x1b00 128]; close $f
    set f [open "wpat_$t.bin" w]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x3800 2048]; close $f
    incr ::n
}
# boot + entrar a gameplay
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
# caminar a la DERECHA (row8 bit7 = 0x80)
after time 13 "keymatrixdown 8 128"
after time 13.4 { snap }
after time 13.7 { snap }
after time 14.0 { snap }
after time 14.3 { snap }
after time 14.6 "keymatrixup 8 128"
# caminar a la IZQUIERDA (row8 bit4 = 0x10)
after time 15.2 "keymatrixdown 8 16"
after time 15.6 { snap }
after time 15.9 { snap }
after time 16.2 { snap }
after time 16.5 "keymatrixup 8 16"
after time 17 { set f [open "dw_done.txt" w]; puts $f "frames $::n"; close $f; exit }
