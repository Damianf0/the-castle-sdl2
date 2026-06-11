# Traza-oraculo del JUGADOR: corre el juego real, inyecta un guion de inputs
# POR FRAME y vuelca el estado del jugador en cada iteracion del game loop
# (bp en 0x4070). Guion: archivo "trace_moves.txt" en cwd, 1 char por frame:
#   R=derecha  L=izquierda  U=SPACE(salto)  D=der+SPACE  A=izq+SPACE
#   W=arriba   S=abajo      .=nada
# Salida: "trace_out.txt": fc spriteY spriteX pattern EAD6 EACB EACC E334 E335
# Uso: openmsx -carta the_castle.rom -script tools/trace_player.tcl  (cwd=repo)
set throttle off

set f [open "trace_moves.txt" r]
set ::moves [string trim [read $f]]
close $f
set ::fc 0
set ::log {}
set ::prev "."

proc keys_of {ch} {
    # devuelve la mascara keymatrix fila 8 para el char del guion
    switch -- $ch {
        R { return 0x80 }
        L { return 0x10 }
        U { return 0x01 }
        D { return 0x81 }
        A { return 0x11 }
        W { return 0x20 }
        S { return 0x40 }
        default { return 0 }
    }
}

proc sample {} {
    set y  [debug read VRAM 0x1b20]
    set x  [debug read VRAM 0x1b21]
    set p  [debug read VRAM 0x1b22]
    set d6 [debug read memory 0xead6]
    set cb [debug read memory 0xeacb]
    set cc [debug read memory 0xeacc]
    set b  [debug read memory 0xe334]
    set c  [debug read memory 0xe335]
    lappend ::log "$::fc $y $x $p $d6 $cb $cc $b $c"

    # aplicar el input de ESTE frame (transicion desde el anterior)
    set ch "."
    if {$::fc < [string length $::moves]} { set ch [string index $::moves $::fc] }
    set old [keys_of $::prev]
    set new [keys_of $ch]
    set rel [expr {$old & ~$new}]
    set prs [expr {$new & ~$old}]
    if {$rel} { keymatrixup   8 $rel }
    if {$prs} { keymatrixdown 8 $prs }
    set ::prev $ch

    incr ::fc
    if {$::fc > [string length $::moves]} {
        debug remove_bp $::bpid
        set f [open "trace_out.txt" w]
        puts $f [join $::log "\n"]
        close $f
        exit
    }
}

# boot -> empezar partida (3 taps de SPACE pasan el titulo)
after time 6   "keymatrixdown 8 1"
after time 6.3 "keymatrixup 8 1"
after time 8   "keymatrixdown 8 1"
after time 8.3 "keymatrixup 8 1"
after time 10  "keymatrixdown 8 1"
after time 10.3 "keymatrixup 8 1"
# margen para que el juego cargue la sala inicial y el jugador quede quieto
after time 14 { set ::bpid [debug set_bp 0x4070 {} { sample }] }
after time 120 { exit }
