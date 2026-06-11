# Captura la pose de salto: salta (hold SPACE) y en el apex vuelca attr+pattern.
# Tambien salto mirando a derecha (camina der + space) y a izquierda.
set throttle off
set ::fc 0
proc grab {tag} {
    set f [open "japat_$tag.bin" w]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x3800 2048]; close $f
    set f [open "jaatr_$tag.bin" w]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x1b00 128]; close $f
}
proc sample {} {
    # salto recto: hold space fc20..30, apex ~fc28 -> grab
    if {$::fc==20} { keymatrixdown 8 1 }
    if {$::fc==28} { grab up }
    if {$::fc==30} { keymatrixup 8 1 }
    # salto a derecha: caminar der fc60.., space fc65..75, apex fc72
    if {$::fc==60} { keymatrixdown 8 128 }
    if {$::fc==65} { keymatrixdown 8 1 }
    if {$::fc==72} { grab right }
    if {$::fc==75} { keymatrixup 8 129 }
    if {$::fc>=90} {
        debug remove_bp $::bpid
        set f [open "jp_done.txt" w]; puts $f done; close $f; exit
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
