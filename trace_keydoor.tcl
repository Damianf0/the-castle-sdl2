# Trazado REAL del sistema de llaves/puertas. Breakpoints en:
#  0x5CA3 = INC (HL) -> 0xE337[idx]   (recoger una llave: incrementa contador)
#  0x75A0 = DEC (HL) -> 0xE337[idx]   (abrir puerta: consume una llave)
# Deja correr el DEMO automático (que juega solo) y registra cada evento con su
# índice de color, registros y posición -> keydoor_log.txt
set throttle off
set ::log {}
proc onkey {} {
    set hl [reg HL]; set idx [expr {$hl - 0xe337}]
    lappend ::log "KEY  idx=$idx A=[reg A] C=[reg C] room=[format %02X [debug read memory 0xe320]] P=[debug read memory 0xe334],[debug read memory 0xe335]"
}
proc ondoor {} {
    set hl [reg HL]; set idx [expr {$hl - 0xe337}]
    set ix [reg IX]
    lappend ::log "DOOR idx=$idx IX1=[debug read memory [expr {$ix+1}]] room=[format %02X [debug read memory 0xe320]] P=[debug read memory 0xe334],[debug read memory 0xe335]"
}
proc dumpe337 {tag} {
    set s "$tag 0xE337="
    for {set i 0} {$i < 14} {incr i} { append s "[debug read memory [expr {0xe337+$i}]] " }
    lappend ::log $s
}
debug set_bp 0x5CA3 {} { onkey }
debug set_bp 0x75A0 {} { ondoor }
# dejar que el demo juegue solo (sin tocar teclas); volcar 0xE337 periódicamente
after time 20 { dumpe337 "T20" }
after time 50 { dumpe337 "T50" }
after time 80 {
    dumpe337 "T80"
    set f [open "keydoor_log.txt" w]; puts $f [join $::log "\n"]; close $f
    set f [open "kd_done.txt" w]; puts $f done; close $f
    exit
}
