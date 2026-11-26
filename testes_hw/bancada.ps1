# bancada.ps1: compila um teste de bancada, grava pela ST-Link e le o resultado da placa.
# Uso (na pasta aviario_luz):
#   powershell -ExecutionPolicy Bypass -File testes_hw\bancada.ps1 -Teste 2            # compila, grava e monitora 60 s
#   powershell -ExecutionPolicy Bypass -File testes_hw\bancada.ps1 -Teste 2 -SoLer     # so le a placa (ja gravada)
#   powershell -ExecutionPolicy Bypass -File testes_hw\bancada.ps1 -Teste 0            # volta o firmware normal
# Testes: 0 normal, 1 chip, 2 LDR, 3 PIR, 4 HC-05, 5 OLED, 6 INA219, 7 I2C, 8 PWM, 9 calibracao (Core/Inc/teste_hw.h).
# Teste 9 (maquete fechada): powershell -ExecutionPolicy Bypass -File testes_hw\bancada.ps1 -Teste 9 -Segundos 40 -Intervalo 1
#   grava a curva duty x LDR e no fim sugere RAW_ESCURO, RAW_CLARO, D_FIXO, KP e P100_mW.
# Precisa: ST-Link no USB e NENHUMA sessao de debug aberta no CubeIDE.
# (Arquivo so em ASCII de proposito: o PowerShell 5.1 estraga acentos sem BOM.)
param(
    [Parameter(Mandatory = $true)][ValidateRange(0, 9)][int]$Teste,
    [switch]$SoLer,          # nao compila nem grava
    [switch]$SoCompilar,     # compila e para (nao grava)
    [int]$Segundos = 60,     # tempo de monitoramento (0 = uma leitura so)
    [int]$Intervalo = 2      # s entre leituras
)
$ErrorActionPreference = 'Stop'
$raiz = Split-Path -Parent $PSScriptRoot
$ide = 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE'
$plug = Join-Path $ide 'plugins'
$cli = (Get-ChildItem $plug -Directory -Filter '*cubeprogrammer.win32*' | Sort-Object Name | Select-Object -Last 1).FullName + '\tools\bin\STM32_Programmer_CLI.exe'
$nm = (Get-ChildItem $plug -Directory -Filter '*gnu-tools-for-stm32*' | Sort-Object Name | Select-Object -Last 1).FullName + '\tools\bin\arm-none-eabi-nm.exe'
$elf = Join-Path $raiz 'Debug\aviario_luz.elf'
$cfg = Join-Path $raiz 'Core\Inc\config.h'
$utf8 = New-Object System.Text.UTF8Encoding($false)

$nomes = @{
    0 = @('firmware normal');
    1 = @('chip', 'flash_kB', 'IDCODE', 'UID0', 'UID1', 'UID2');
    2 = @('LDR', 'raw', 'min', 'max', 'pct', 'faixa(max-min)', 's_no_extremo', 'mV');
    3 = @('PIR', 'nivel_PA1', 'bordas', 'contagem_pir.c', 's_ligado', 'inspecao');
    4 = @('HC-05', 'pings', 'bytes_rx', 'ultimo_byte', 'erros_tx', 'erros_rx');
    5 = @('OLED', 'achou_0x3C', 'init_ok', 'envios_ok', 'envios_falha');
    6 = @('INA219', 'achou_0x40', 'init_ok', 'leitura_ok', 'Vbus_mV', 'I_mA', 'P_mW', 'falhas');
    7 = @('I2C', 'mapa0', 'mapa1', 'mapa2', 'mapa3', 'quantos', 'tem_0x3C', 'tem_0x40');
    8 = @('PWM', 'duty', 'tem_INA', 'I_mA', 'P_mW', 'I_max', 'I_50pct', 'I_100pct');
    9 = @('CALIB', 'duty', 'raw', 'degraus', 'duty_degrau', 'raw_medio', 'ruido', 'I_mA', 'P_mW')
}

function Define-Teste([int]$n) {
    $txt = [IO.File]::ReadAllText($cfg, $utf8)
    $novo = [regex]::Replace($txt, '(#define\s+TESTE_HW\s+)\d+', "`${1}$n")
    if ($novo -ne $txt) { [IO.File]::WriteAllText($cfg, $novo, $utf8) }
}

function Compila {
    $ws = Join-Path $env:TEMP ('aviario_ws_' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    Write-Host "Compilando (TESTE_HW = $Teste)..."
    $log = & "$ide\stm32cubeidec.exe" -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
        -data $ws -import $raiz -cleanBuild aviario_luz/Debug 2>&1 | Out-String
    Remove-Item $ws -Recurse -Force -ErrorAction SilentlyContinue
    $erros = ([regex]::Matches($log, '(?m)error:')).Count
    $avisos = ([regex]::Matches($log, '(?m)warning:')).Count
    $tam = [regex]::Match($log, '(?m)^\s*\d+\s+\d+\s+\d+\s+\d+\s+[0-9a-f]+\s+aviario_luz\.elf.*$').Value.Trim()
    if ($erros -gt 0 -or -not (Test-Path $elf)) {
        $log -split "`n" | Select-String 'error:' | Select-Object -First 15 | ForEach-Object { Write-Host $_ }
        throw "Build falhou ($erros erros)."
    }
    Write-Host "Build ok: $erros erros, $avisos avisos.  text data bss: $tam"
    $log -split "`n" | Select-String 'warning:' | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" }
}

function Grava {
    Write-Host 'Gravando pela ST-Link...'
    $out = & $cli -c port=SWD freq=4000 -w $elf -v -rst 2>&1 | Out-String
    if ($out -notmatch 'Download verified successfully') {
        Write-Host 'Tentando de novo com connect under reset...'
        $out = & $cli -c port=SWD mode=UR freq=4000 -w $elf -v -rst 2>&1 | Out-String
    }
    if ($out -notmatch 'Download verified successfully') {
        $out -split "`n" | Select-String 'Error|error|ST-LINK|Flash' | Select-Object -First 12 | ForEach-Object { Write-Host $_ }
        throw 'Gravacao falhou. ST-Link no USB? Debug do CubeIDE fechado? Fios SWDIO/SWCLK/GND/3V3?'
    }
    Write-Host 'Gravado e verificado; placa reiniciada.'
}

function Endereco {
    $linha = & $nm $elf | Select-String '\steste_res$'
    if (-not $linha) { return $null }
    return '0x' + ($linha.Line -split '\s+')[0]
}

function Le([string]$end) {
    $out = & $cli -c port=SWD mode=HOTPLUG -r32 $end 0x30 2>&1 | Out-String
    $pal = @()
    foreach ($m in [regex]::Matches($out, '(?m)^\s*0x[0-9A-Fa-f]{8}\s*:\s*((?:[0-9A-Fa-f]{8}\s*)+)$')) {
        $pal += ($m.Groups[1].Value.Trim() -split '\s+')
    }
    if ($pal.Count -lt 12) { return $null }
    return $pal | ForEach-Object { [Convert]::ToInt32($_, 16) }
}

function Mostra($w) {
    $est = @('RODANDO', 'OK', 'FALHA')
    if ($w[0] -ne 0x7E57B00C) {
        $m = '{0:X8}' -f $w[0]
        Write-Host "$(Get-Date -Format HH:mm:ss)  sem teste rodando (magic $m): firmware normal ou placa reiniciando"
        return
    }
    $id = $w[1]
    $n = $nomes[[int]$id]
    $s = "$(Get-Date -Format HH:mm:ss)  T$id $($n[0])  [$($est[[int]$w[2]])]  ciclos=$($w[3])"
    for ($k = 1; $k -lt $n.Count; $k++) {
        $v = $w[3 + $k]
        if ((($id -eq 1) -and ($k -ge 2)) -or (($id -eq 7) -and ($k -le 4))) { $v = '0x{0:X8}' -f $v }
        $s += "  $($n[$k])=$v"
    }
    if ($id -eq 7) {
        $ach = @()
        for ($a = 8; $a -le 0x77; $a++) { if ((($w[4 + ($a -shr 5)] -shr ($a -band 31)) -band 1) -eq 1) { $ach += ('0x{0:X2}' -f $a) } }
        $s += "  enderecos: $($ach -join ' ')"
    }
    Write-Host $s
}

# ---- fluxo ----
if (-not $SoLer) {
    try { Define-Teste $Teste; Compila } finally { Define-Teste 0 }   # o repositorio fica sempre em 0
    if ($SoCompilar) { exit 0 }
    Grava
}
if ($Teste -eq 0) { Write-Host 'Firmware normal gravado; nada para monitorar aqui.'; exit 0 }
$end = Endereco
if (-not $end) { throw 'teste_res nao esta no .elf (compile com o teste antes, sem -SoLer).' }
Write-Host "teste_res em $end. Lendo a cada $Intervalo s por $Segundos s (Ctrl+C para parar)."
Start-Sleep -Milliseconds 800
$fim = (Get-Date).AddSeconds($Segundos)
$pontos = @{}          # teste 9: duty do degrau -> ultimo ponto medido
$degrau = -1
do {
    $w = Le $end
    if ($w) {
        Mostra $w
        if (($w[1] -eq 9) -and ($w[0] -eq 0x7E57B00C) -and ($w[6] -ne $degrau)) {
            $degrau = $w[6]
            if ($degrau -gt 0) {
                $pontos[[int]$w[7]] = [pscustomobject]@{ duty = [int]$w[7]; raw = [int]$w[8]; ruido = [int]$w[9]; I = [int]$w[10]; P = [int]$w[11] }
            }
        }
    } else { Write-Host "$(Get-Date -Format HH:mm:ss)  leitura falhou (ST-Link ocupado ou desconectado)" }
    if ($Segundos -le 0) { break }
    Start-Sleep -Seconds $Intervalo
} while ((Get-Date) -lt $fim)

# ---- teste 9: curva e sugestao de calibracao ----
if ($Teste -eq 9) {
    $c = $pontos.Values | Sort-Object duty
    if ($c.Count -lt 11) {
        Write-Host "So $($c.Count) de 11 degraus gravados: rode de novo com -Segundos 40 -Intervalo 1."
        if ($c.Count -lt 2) { exit 1 }
    }
    $txt = [IO.File]::ReadAllText($cfg, $utf8)
    $sp = [int][regex]::Match($txt, '#define\s+SP_DIA\s+(\d+)').Groups[1].Value
    $p0 = $c | Where-Object { $_.duty -eq 0 }
    $p100 = $c | Where-Object { $_.duty -eq 1000 }
    Write-Host ''
    Write-Host 'Curva (media de cada degrau):'
    Write-Host '  duty%   raw   ruido    mA     mW'
    foreach ($p in $c) { Write-Host ('  {0,4}  {1,5}  {2,5}  {3,5}  {4,6}' -f ($p.duty / 10), $p.raw, $p.ruido, $p.I, $p.P) }
    $avisos = @()
    for ($k = 1; $k -lt $c.Count; $k++) { if ($c[$k].raw -lt $c[$k - 1].raw - 30) { $avisos += "raw caiu de $($c[$k-1].duty/10)% para $($c[$k].duty/10)%: luz de fora mudando ou mau contato" } }
    foreach ($p in $c) {
        if ($p.ruido -gt 150) { $avisos += "ruido de $($p.ruido) a $($p.duty/10)%: mau contato no LDR ou maquete aberta" }
        if (($p.raw -eq 0) -and ($p.duty -gt 0)) { $avisos += "raw 0 com a fita a $($p.duty/10)%: LDR aberto (fio solto)" }
    }
    if ($p0 -and $p100) {
        $esc = $p0.raw; $cla = $p100.raw
        if ($cla -gt 3700) { $avisos += "RAW_CLARO $cla acima de 3700: LDR perto de saturar (afastar o LDR ou 10k -> 5k)" }
        if (($cla - $esc) -lt 1000) { $avisos += "faixa $($cla - $esc) abaixo de 1000 (T1 falha): LDR longe ou sem ver a fita" }
        # D_FIXO: duty em que a luz da $sp % (interpolacao linear entre degraus)
        $alvo = $esc + ($cla - $esc) * $sp / 100.0
        $dfixo = $null; $g = $null
        for ($k = 1; $k -lt $c.Count; $k++) {
            $a = $c[$k - 1]; $b = $c[$k]
            if (($a.raw -le $alvo) -and ($b.raw -ge $alvo) -and ($b.raw -gt $a.raw)) {
                $dfixo = $a.duty + ($b.duty - $a.duty) * ($alvo - $a.raw) / ($b.raw - $a.raw)
                $g = (($b.raw - $a.raw) * 100.0 / ($cla - $esc)) / ($b.duty - $a.duty)   # % de luz por permil, local
                break
            }
        }
        Write-Host ''
        Write-Host 'Sugestao para o config.h (provisoria: confira os avisos):'
        Write-Host "  RAW_ESCURO  $esc"
        Write-Host "  RAW_CLARO   $cla"
        if ($dfixo -ne $null) {
            $kp = [math]::Max(1, [math]::Round(0.5 / $g))
            Write-Host ('  D_FIXO      {0}   (duty que da {1}% de luz)' -f [math]::Round($dfixo), $sp)
            Write-Host ('  KP          {0}   (ganho local {1:0.000} %/permil; KP*G = {2:0.00}, alvo 0,5, tem que ficar < 1)' -f $kp, $g, ($kp * $g))
        } else { $avisos += "nao achei o duty que da $sp% de luz (curva sem cruzar o alvo)" }
        if ($p100.P -gt 0) { Write-Host "  P100_mW     $($p100.P)" }
    }
    if ($avisos.Count) { Write-Host ''; Write-Host 'Avisos:'; $avisos | Select-Object -Unique | ForEach-Object { Write-Host "  - $_" } }
}
