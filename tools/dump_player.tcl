# Entra a gameplay real y vuelca la sprite attribute table (0x1B00),
# la sprite pattern table (0x3800) y RAM relevante para identificar al jugador.
set throttle off

proc dumpall {tag} {
    set f [open [format "spr_attr_%s.bin" $tag] w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x1b00 128]
    close $f
    set f [open [format "spr_pat_%s.bin" $tag] w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x3800 2048]
    close $f
    set f [open [format "ram_%s.bin" $tag] w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block memory 0xe000 0x600]
    close $f
    set f [open [format "info_%s.txt" $tag] w]
    puts $f "intro_flag(0xEAE4)=[debug read memory 0xeae4]"
    puts $f "room(0xE320)=[debug read memory 0xe320]"
    close $f
}

# entrar al juego: pulsar espacio varias veces tras el boot
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
# volcar en dos instantes por si el jugador parpadea / anima
after time 16 { dumpall "a" }
after time 18 { dumpall "b" }
after time 18.5 { set f [open "dp_done.txt" w]; puts $f done; close $f; exit }
