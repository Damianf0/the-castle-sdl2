# Captura las trazas-oraculo del jugador (Fase 3) corriendo openMSX con
# distintos guiones de input. Salida: tests/fixtures/traces/trace_<nombre>.txt
# (formato: fc spriteY spriteX pattern EAD6 EACB EACC E334 E335 por frame).
# Uso: powershell -ExecutionPolicy Bypass -File tools\capture_traces.ps1
$ErrorActionPreference = 'Stop'
$openmsx = "..\_buildtools\openmsx\openmsx.exe"
$outdir = "tests\fixtures\traces"
New-Item -ItemType Directory -Force $outdir | Out-Null

$scripts = [ordered]@{
    # quieto (linea base: gravedad/spawn)
    idle      = ('.' * 60)
    # caminar a la derecha 40 frames, parar, izquierda 40
    walk      = ('.' * 8) + ('R' * 40) + ('.' * 10) + ('L' * 40) + ('.' * 10)
    # salto corto (tap de 2 frames) parado
    jumptap   = ('.' * 8) + ('U' * 2) + ('.' * 60)
    # salto sostenido (hold largo) parado
    jumphold  = ('.' * 8) + ('U' * 30) + ('.' * 50)
    # salto en carrera a la derecha (corto: en 0x70 hay un arquero cerca)
    runjump   = ('.' * 8) + ('R' * 4) + ('D' * 10) + ('.' * 25)
    # caminar contra la pared izquierda
    wall      = ('.' * 8) + ('L' * 60) + ('.' * 10)
}

foreach ($name in $scripts.Keys) {
    Set-Content trace_moves.txt $scripts[$name] -NoNewline -Encoding ascii
    if (Test-Path trace_out.txt) { Remove-Item trace_out.txt }
    & $openmsx -carta the_castle.rom -script tools\trace_player.tcl | Out-Null
    if (-not (Test-Path trace_out.txt)) { Write-Host "FALLO: $name" -ForegroundColor Red; exit 1 }
    Move-Item -Force trace_out.txt "$outdir\trace_$name.txt"
    Copy-Item trace_moves.txt "$outdir\moves_$name.txt"
    Write-Host "OK: trace_$name.txt ($($scripts[$name].Length) frames)"
}
Remove-Item trace_moves.txt
Write-Host "Trazas en $outdir"
