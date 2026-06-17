# Oraculo de la SECUENCIA DE NOTAS in-game (sala 01, jugador vivo). Tras
# asentar (frame>120) loguea en sub_765C "canal:nota" (0=A,2=B) hasta juntar
# 240 eventos. Salida -> tr_musicseq_out.txt. El port (CASTLE_MUSICTRACE)
# debe reproducir esta misma secuencia (subsecuencia ciclica, por fase).
set throttle off
set ::log {}
set ::fr 0
proc fixplayer {} {
    debug write memory 0xe334 14
    debug write memory 0xe335 11
    debug write memory 0xeae0 0
    debug write memory 0xe343 0x7F
    incr ::fr
}
proc atnote {} {
    if {$::fr < 120} return
    lappend ::log "[reg C]:[format %02X [reg A]]"
    if {[llength $::log] >= 240} {
        set f [open "tr_musicseq_out.txt" w]; puts $f [join $::log "\n"]; close $f
        exit
    }
}
proc forceload {} {
    debug write memory 0xe320 0x01
    debug write memory 0xe334 14
    debug write memory 0xe335 11
    set sp [expr {[reg SP] - 2}]
    debug write memory $sp 0x70
    debug write memory [expr {$sp + 1}] 0x40
    reg SP $sp
    reg PC 0x64DD
    debug set_bp 0x4070 {} { fixplayer }
    debug set_bp 0x765C {} { atnote }
}
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
after time 16 { forceload }
after time 120 {
    if {[llength $::log] > 0} {
        set f [open "tr_musicseq_out.txt" w]; puts $f [join $::log "\n"]; close $f
    }
    exit
}
