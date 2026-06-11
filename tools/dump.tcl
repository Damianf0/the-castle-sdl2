# openMSX: bootea The Castle, entra al juego (espacio) y vuelca la VRAM real.
set throttle off
# arrancar el juego: pulsar espacio un par de veces pasado el boot/titulo
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 14 {
    set rx [debug read memory 0xE320]
    set ry [debug read memory 0xE321]
    set intro [debug read memory 0xEAE4]
    set f [open "dumpinfo.txt" w]
    puts $f "room_x(0xE320)=$rx room_y(0xE321)=$ry intro(0xEAE4)=$intro"
    close $f
    set f [open "vram.bin" w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0 16384]
    close $f
    set f [open "ram.bin" w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block memory 0xE000 0x1000]
    close $f
    exit
}
