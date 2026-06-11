# Trace largo en modo demo/gameplay natural: el demo navega solo por las salas.
# Loguea sala + tabla BAT (0xE416) cada frame -> demo_trace.txt. Luego se
# extraen segmentos por sala/tipo de enemigo.
set throttle off
set ::fc 0
set ::log {}
proc sample {} {
    set rm [debug read memory 0xe320]
    set line "$::fc R[format %02X $rm] P[debug read memory 0xe334],[debug read memory 0xe335]"
    for {set s 0} {$s < 8} {incr s} {
        set base [expr {0xe416 + $s*5}]
        set tp [debug read memory [expr {$base+1}]]
        if {$tp != 0} {
            append line " | t[format %02X $tp] r[debug read memory [expr {$base+2}]] c[debug read memory [expr {$base+3}]] f[debug read memory [expr {$base+4}]]"
        }
    }
    lappend ::log $line
    if {$::fc >= 3000} {
        debug remove_bp $::bpid
        set f [open "demo_trace.txt" w]; puts $f [join $::log "\n"]; close $f
        set f [open "demo_done.txt" w]; puts $f done; close $f
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
