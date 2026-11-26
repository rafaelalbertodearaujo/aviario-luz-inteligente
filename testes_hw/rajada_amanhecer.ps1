param([int]$Iter = 24, [int]$N = 1000, [string]$Pasta = 'rajada')
$ErrorActionPreference = 'Stop'
$raiz = 'C:\Users\Rafael\Desktop\work\aviario_luz'
$plug = 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins'
$cli = (Get-ChildItem $plug -Directory -Filter '*cubeprogrammer.win32*' | Sort-Object Name | Select-Object -Last 1).FullName + '\tools\bin\STM32_Programmer_CLI.exe'
$nm = (Get-ChildItem $plug -Directory -Filter '*gnu-tools-for-stm32*' | Sort-Object Name | Select-Object -Last 1).FullName + '\tools\bin\arm-none-eabi-nm.exe'
$elf = Join-Path $raiz 'Debug\aviario_luz.elf'
$linha = & $nm $elf | Select-String '\sestado$'
$end = '0x' + ($linha.Line -split '\s+')[0]
New-Item -ItemType Directory -Force $Pasta | Out-Null

function MinutoAgora {
    $out = & $cli -c port=SWD mode=HOTPLUG -r8 $end 8 2>&1 | Out-String
    $m = [regex]::Match($out, '(?m)^\s*0x[0-9A-Fa-f]{8}\s*:\s*([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})')
    if ($m.Success) { return [Convert]::ToInt32($m.Groups[1].Value, 16) + 256 * [Convert]::ToInt32($m.Groups[2].Value, 16) }
    return -1
}

# espera ficar de noite perto do fim (a partir de 04:30) para pegar o amanhecer inteiro
for ($i = 0; $i -lt 6000; $i++) {
    $min = MinutoAgora
    if ($min -ge 270 -and $min -lt 300) { Write-Output "disparando em $min min do galpao"; break }
}

$args2 = @('-c', 'port=SWD', 'mode=HOTPLUG')
for ($i = 0; $i -lt $N; $i++) { $args2 += @('-r8', $end, '44') }
for ($k = 0; $k -lt $Iter; $k++) {
    $t0 = Get-Date
    & $cli @args2 2>&1 | Out-File (Join-Path $Pasta ("r{0:00}.txt" -f $k)) -Encoding ascii
    ('{0:00} {1}' -f $k, $t0.ToString('HH:mm:ss.fff')) | Out-File (Join-Path $Pasta 'tempos.txt') -Append -Encoding ascii
}
Write-Output 'fim'
