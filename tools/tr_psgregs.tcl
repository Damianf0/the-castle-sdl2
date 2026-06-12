# Lee el estado de los registros PSG durante el juego (para ver R11/R12/R7
# que el juego nunca escribe: los dejo el BIOS). Salida: tr_psgregs_out.txt
set throttle off
proc forceload {} {
    debug write memory 0xe320 0x01
    debug write memory 0xe334 14
    debug write memory 0xe335 11
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
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 16 { forceload }
after time 19 {
    set out {}
    for {set r 0} {$r < 16} {incr r} {
        lappend out "R$r=[debug read {PSG regs} $r]"
    }
    set f [open "tr_psgregs_out.txt" w]; puts $f [join $out "\n"]; close $f
    exit
}
after time 60 {
    set f [open "tr_psgregs_out.txt" w]; puts $f "timeout"; close $f
    exit
}
