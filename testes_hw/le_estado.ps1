# le_estado.ps1: le pela ST-Link o retrato do firmware normal (estado_t em app.c), sem parar a placa.
# Uso (na pasta aviario_luz, com o firmware normal gravado e o Debug\aviario_luz.elf dele):
#   powershell -ExecutionPolicy Bypass -File testes_hw\le_estado.ps1 -Segundos 30 -Intervalo 2
# Serve para a calibracao: raw do LDR, luz %, duty, corrente e potencia da fita.
# Offsets do estado_t conferidos no DWARF do .elf em 18/11 (enums de 1 byte). Se mudar o app.h, conferir de novo.
# (Arquivo so em ASCII de proposito: o PowerShell 5.1 estraga acentos sem BOM.)
param(
    [int]$Segundos = 30,
    [int]$Intervalo = 2
)
$ErrorActionPreference = 'Stop'
$raiz = Split-Path -Parent $PSScriptRoot
$plug = 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins'
$cli = (Get-ChildItem $plug -Directory -Filter '*cubeprogrammer.win32*' | Sort-Object Name | Select-Object -Last 1).FullName + '\tools\bin\STM32_Programmer_CLI.exe'
$nm = (Get-ChildItem $plug -Directory -Filter '*gnu-tools-for-stm32*' | Sort-Object Name | Select-Object -Last 1).FullName + '\tools\bin\arm-none-eabi-nm.exe'
$elf = Join-Path $raiz 'Debug\aviario_luz.elf'

$linha = & $nm $elf | Select-String '\sestado$'
if (-not $linha) { throw 'estado nao esta no .elf (compile o firmware normal).' }
$end = '0x' + ($linha.Line -split '\s+')[0]

function U16($b, $o) { return [int]$b[$o] + 256 * [int]$b[$o + 1] }
function S16($b, $o) { $v = U16 $b $o; if ($v -ge 32768) { $v -= 65536 }; return $v }
function U32($b, $o) { return [int64](U16 $b $o) + 65536 * [int64](U16 $b ($o + 2)) }

Write-Host "estado em $end. Lendo a cada $Intervalo s por $Segundos s."
$fim = (Get-Date).AddSeconds($Segundos)
$hist = @()    # para o resumo do fim (media, minimo, maximo)
do {
    $out = & $cli -c port=SWD mode=HOTPLUG -r8 $end 44 2>&1 | Out-String
    $b = @()
    foreach ($m in [regex]::Matches($out, '(?m)^\s*0x[0-9A-Fa-f]{8}\s*:\s*((?:[0-9A-Fa-f]{2}\s*)+)$')) {
        $b += ($m.Groups[1].Value.Trim() -split '\s+' | ForEach-Object { [Convert]::ToInt32($_, 16) })
    }
    if ($b.Count -lt 44) {
        Write-Host "$(Get-Date -Format HH:mm:ss)  leitura falhou (ST-Link ocupado ou desconectado)"
    } else {
        $min = U16 $b 0
        $hora = '{0:00}:{1:00}' -f [math]::Floor($min / 60), ($min % 60)
        $modo = if ($b[3] -eq 1) { 'MANUAL' } else { 'AUTO' }
        $per = if ($b[4]) { 'NOITE' } else { 'DIA' }
        $insp = if ($b[5]) { ' INSPECAO' } else { '' }
        Write-Host ("{0}  {1} {2,-5} {3,-6}{4}  alvo={5}%  raw={6}  luz={7}%  duty={8:0.0}%  I={9}mA  P={10}mW  PIR={11}" -f `
            (Get-Date -Format HH:mm:ss), $hora, $per, $modo, $insp, $b[6], (U16 $b 8), (S16 $b 10), ((U16 $b 12) / 10.0), (S16 $b 18), (U32 $b 20), (U16 $b 40))
        $hist += [pscustomobject]@{ raw = (U16 $b 8); luz = (S16 $b 10); duty = (U16 $b 12) / 10.0; I = (S16 $b 18); P = (U32 $b 20) }
    }
    if ($Segundos -le 0) { break }
    Start-Sleep -Seconds $Intervalo
} while ((Get-Date) -lt $fim)

# Resumo: a media de varias leituras vale mais que uma so (o LDR oscila algumas dezenas).
if ($hist.Count -ge 3) {
    Write-Host ''
    Write-Host "Resumo de $($hist.Count) leituras (media / min / max):"
    foreach ($campo in 'raw', 'luz', 'duty', 'I', 'P') {
        $m = $hist | Measure-Object -Property $campo -Average -Minimum -Maximum
        Write-Host ('  {0,-5} {1,8:0.0} {2,8} {3,8}' -f $campo, $m.Average, $m.Minimum, $m.Maximum)
    }
    $r = $hist | Measure-Object -Property raw -Minimum -Maximum
    if (($r.Maximum - $r.Minimum) -gt 150) { Write-Host '  Aviso: raw variou mais de 150. Luz de fora mudando, maquete aberta ou mau contato no LDR.' }
    if ((($hist | Where-Object { $_.raw -eq 0 -and $_.duty -gt 0 }).Count) -gt 0) { Write-Host '  Aviso: raw 0 com a fita acesa. LDR aberto (fio solto).' }
}
