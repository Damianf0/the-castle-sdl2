# Fuerza cada sala (sub_64DD) y vuelca la VRAM completa (16KB) -> vram_XX.bin
set throttle off
set ::rooms {}
for {set hi 0} {$hi <= 9} {incr hi} {
    for {set lo 0} {$lo <= 9} {incr lo} { lappend ::rooms [expr {$hi*16 + $lo}] }
}
set ::idx -1
proc dumpvram {rx} {
    set f [open [format "vram_%02X.bin" $rx] w]
    fconfigure $f -translation binary
    puts -nonewline $f [debug read_block VRAM 0 16384]
    close $f
}
proc step {} {
    if {$::idx >= 0} { dumpvram [debug read memory 0xE320] }
    incr ::idx
    if {$::idx >= [llength $::rooms]} {
        debug remove_bp $::bpid
        set f [open "cap_done.txt" w]; puts $f "done"; close $f
        exit
    }
    set r [lindex $::rooms $::idx]
    debug write memory 0xE320 $r
    set sp [expr {[reg SP] - 2}]
    debug write memory $sp 0x70
    debug write memory [expr {$sp + 1}] 0x40
    reg SP $sp
    reg PC 0x64DD
}
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 11 { set ::bpid [debug set_bp 0x4070 {} { step }] }
