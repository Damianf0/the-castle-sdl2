# Mapea duracion de hold de SPACE -> trayectoria de salto. Tambien SPACE+DER.
set throttle off
set ::fc 0
set ::log {}
set ::tests {}
set s 20
foreach hold {1 2 4 6 8 12 20} {
    lappend ::tests [list $s $hold 1]
    set s [expr {$s + 45}]
}
lappend ::tests [list $s 8 129]
set ::endfc [expr {$s + 45}]
proc sample {} {
    set y [debug read VRAM 0x1b20]; set x [debug read VRAM 0x1b21]
    lappend ::log "$::fc $y $x"
    foreach t $::tests {
        lassign $t f h m
        if {$::fc == $f} { keymatrixdown 8 $m }
        if {$::fc == [expr {$f + $h}]} { keymatrixup 8 $m }
    }
    if {$::fc >= $::endfc} {
        debug remove_bp $::bpid
        set fp [open "jump_trace3.txt" w]; puts $fp [join $::log "\n"]; close $fp
        set fp [open "tj3_done.txt" w]; puts $fp done; close $fp
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
