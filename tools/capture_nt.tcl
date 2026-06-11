# Vuelca la NAME TABLE original (VRAM 0x1800, 768 bytes, Ã­ndices de tile 0-255
# SIN deduplicar) de las 100 salas -> ont_XX.bin. Es el dato que usa la lÃ³gica
# real del juego (llaves = tile 0x2A+color, puertas, etc.).
set throttle off
set ::rooms {}
for {set hi 0} {$hi <= 9} {incr hi} {
    for {set lo 0} {$lo <= 9} {incr lo} { lappend ::rooms [expr {$hi*16 + $lo}] }
}
set ::idx -1
proc dumpnt {rx} {
    set f [open [format "tests/fixtures/ont/ont_%02X.bin" $rx] w]; fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0x1800 768]; close $f
}
proc step {} {
    if {$::idx >= 0} { dumpnt [debug read memory 0xe320] }
    incr ::idx
    if {$::idx >= [llength $::rooms]} {
        debug remove_bp $::bpid
        set f [open "ont_done.txt" w]; puts $f done; close $f; exit
    }
    set r [lindex $::rooms $::idx]
    debug write memory 0xe320 $r
    set sp [expr {[reg SP]-2}]; debug write memory $sp 0x70; debug write memory [expr {$sp+1}] 0x40
    reg SP $sp; reg PC 0x64DD
}
after time 6 "keymatrixdown 8 1"; after time 6.3 "keymatrixup 8 1"
after time 8 "keymatrixdown 8 1"; after time 8.3 "keymatrixup 8 1"
after time 11 { set ::bpid [debug set_bp 0x4070 {} { step }] }
