$p = New-Object System.IO.Ports.SerialPort 'COM11',9600,'None',8,'One'
$p.ReadTimeout = 300
try {
  $p.Open(); Start-Sleep -Milliseconds 500
  $null = $p.ReadExisting()
  $p.Write('P')
  $fim = (Get-Date).AddSeconds(320)
  $resto = ''; $achou = $false; $ultimaHora = ''
  while ((Get-Date) -lt $fim -and -not $achou) {
    Start-Sleep -Milliseconds 200
    $resto += $p.ReadExisting()
    while ($resto.Contains("`n") -and -not $achou) {
      $i = $resto.IndexOf("`n"); $linha = $resto.Substring(0,$i).Trim(); $resto = $resto.Substring($i+1)
      if ($linha -match 'hora=(\d\d):(\d\d)') {
        $h = [int]$Matches[1]; $ultimaHora = "$($Matches[1]):$($Matches[2])"
        if ($h -ge 23 -or $h -lt 5) { $p.Write('F'); $achou = $true; Write-Output "NOITE detectada em $ultimaHora; mandei F: $linha" }
      }
    }
  }
  if (-not $achou) { Write-Output "Nao chegou a noite em 320 s (ultima hora $ultimaHora)" }
  Start-Sleep -Milliseconds 1500
  $rest = $p.ReadExisting()
  Write-Output ("Depois do F: " + (($rest -split "`n" | Select-Object -First 4) -join ' | '))
  $p.Write('T'); Start-Sleep -Milliseconds 1200
  Write-Output ("T: " + $p.ReadExisting().Trim())
} catch { Write-Output "ERRO: $($_.Exception.Message)" }
finally { if ($p.IsOpen) { $p.Close() } }
