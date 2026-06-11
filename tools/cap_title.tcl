# Captura la VRAM del TITULO real: breakpoint en sub_4AD7 (fase 3 de la intro,
# "esperar input") — en ese momento el logo y los creditos estan completos y la
# pantalla queda estatica 128 frames. -> tests/fixtures/vram_title.bin
# Uso: openmsx -carta the_castle.rom -script tools/cap_title.tcl  (cwd = repo)
set throttle off
set ::log [open "captitle_log.txt" w]
puts $::log "script cargado, cwd=[pwd]"
flush $::log
set ::done 0
set ::bpid [debug set_bp 0x4AD7 {} {
    if {!$::done} {
        set ::done 1
        puts $::log "bp 0x4AD7 disparado"
        set f [open "tests/fixtures/vram_title.bin" w]
        fconfigure $f -translation binary
        puts -nonewline $f [debug read_block VRAM 0 16384]
        close $f
        puts $::log "vram_title.bin escrito"
        close $::log
        debug remove_bp $::bpid
        exit
    }
}]
# failsafe por si el bp nunca dispara
after time 60 {
    puts $::log "FAILSAFE: bp nunca disparo, PC=[reg PC]"
    close $::log
    exit
}
