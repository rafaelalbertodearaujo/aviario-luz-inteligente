# bt.ps1: manda comandos de uma letra pelo Bluetooth (HC-05, COM11) e mostra a resposta de cada um.
# Uso (o painel do hub e o PuTTY precisam estar FECHADOS: a porta aceita um programa so):
#   powershell -ExecutionPolicy Bypass -File testes_hw\bt.ps1 -Comandos '?','S','A' -EsperaMs 1500
# Nunca mande W (trava a placa de proposito). P liga o fluxo do painel (1 linha por segundo): T volta ao resumo.
# Exemplo de teste do PIR a noite: rode congela_noite.ps1 (espera a noite do ciclo e manda F), passe a mao no PIR
# olhando o le_estado.ps1, e depois mande F de novo para o relogio voltar a andar.
# (Arquivo so em ASCII de proposito: o PowerShell 5.1 estraga acentos sem BOM.)
param(
    [Parameter(Mandatory = $true)][string[]]$Comandos,
    [string]$Porta = 'COM11',
    [int]$EsperaMs = 1500
)
$p = New-Object System.IO.Ports.SerialPort $Porta, 9600, 'None', 8, 'One'
$p.ReadTimeout = 300
try {
    $p.Open()
    Start-Sleep -Milliseconds 500
    $null = $p.ReadExisting()
    foreach ($c in $Comandos) {
        if ($c -ceq 'W') { Write-Output "ignorado: W trava a placa de proposito"; continue }
        $null = $p.ReadExisting()
        $p.Write($c)
        Start-Sleep -Milliseconds $EsperaMs
        Write-Output ("--- '{0}' ->" -f $c)
        Write-Output ($p.ReadExisting().Trim())
    }
} catch {
    Write-Output ("ERRO na {0}: {1}" -f $Porta, $_.Exception.Message)
    Write-Output 'Se for "acesso negado": feche o painel do hub no Chrome e o PuTTY.'
} finally {
    if ($p.IsOpen) { $p.Close() }
}
