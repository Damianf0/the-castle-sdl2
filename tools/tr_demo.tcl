# Oraculo de la DEMO: arranca el juego y NO toca ninguna tecla — el titulo
# da sus 3 ciclos y entra solo al demo mode (input grabado en 0x7ABE). El
# bp en 0x4070 (game loop) solo dispara durante la demo. Traza por frame:
# sala, jugador, fase, vidas, input aplicado y puntero del stream.
# Corta cuando EAE3=1 (fin del stream) o al limite de frames.
# Salida -> tr_demo_out.txt (mismo formato que CASTLE_DEMOTRACE).
set throttle off
set ::fc -1
set ::log {}
set ::prevptr 0
proc sample {} {
    if {$::fc == -1} { set ::fc 0 }
    set ptr [expr {[debug read memory 0xeae5] | ([debug read memory 0xeae6] << 8)}]
    # el puntero retrocedio a 0x7ABE => la demo REINICIO (la anterior
    # termino con la muerte de la IA): cortar sin loguear el run nuevo
    if {$ptr < $::prevptr} { finish }
    set ::prevptr $ptr
    lappend ::log "$::fc sala=[format %02X [debug read memory 0xe320]] plr=[debug read memory 0xe334],[debug read memory 0xe335] fase=[format %02X [debug read memory 0xead6]] vidas=[debug read memory 0xe324],[debug read memory 0xe336] in=[debug read memory 0xeacb],[expr {[debug read memory 0xeacc] != 0}] ptr=[format %02X%02X [debug read memory 0xeae6] [debug read memory 0xeae5]] r=[debug read memory 0xeae7]"
    if {[debug read memory 0xeae3] != 0 || $::fc >= 8000} { finish }
    incr ::fc
}
proc finish {} {
    set f [open "tr_demo_out.txt" w]; puts $f [join $::log "\n"]; close $f
    set f [open "tr_demo_done.txt" w]; puts $f done; close $f
    exit
}
after time 16 {
    set ::bpid [debug set_bp 0x4070 {} { sample }]
}
after time 1200 {
    if {[llength $::log] > 0} { finish }
    set f [open "tr_demo_err.txt" w]; puts $f "timeout sin demo fc=$::fc"; close $f
    exit
}
