# =============================================================================
# Testes de integração da JVM no Windows (PowerShell).
#
# Pré-requisito: os .class já compilados no Linux/WSL com
#   tests/run_tests.sh --compile-only
# e copiados junto com tests/expected e jvm.exe.
#
# Uso (na raiz do projeto):
#   powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 [-Jvm build\jvm.exe] [-Filter texto]
# =============================================================================
param(
    [string]$Jvm = "build\jvm.exe",
    [string]$ClassesDir = "build\tests\classes",
    [string]$ExpectedDir = "tests\expected",
    [string]$Filter = ""
)

# A JVM escreve UTF-8 e as saídas esperadas são UTF-8 sem BOM: sem isto o
# Windows PowerShell 5.1 decodifica a saída como OEM e os arquivos como ANSI,
# e os testes com acentos falhariam.
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$pass = 0
$fail = 0

foreach ($versionDir in Get-ChildItem -Directory $ClassesDir) {
    # Cada teste é um diretório que contém javac.log: <ver>\<tema>\<Nome>
    foreach ($log in Get-ChildItem -Recurse -Filter "javac.log" $versionDir.FullName) {
        $testDir = $log.Directory
        $relative = $testDir.FullName.Substring($versionDir.FullName.Length + 1)
        if ($Filter -and -not $relative.Contains($Filter)) { continue }
        $name = $testDir.Name
        $label = "$relative [$($versionDir.Name)]"

        $expectedTxt = Join-Path $ExpectedDir "$relative.txt"
        $expectedCodeFile = Join-Path $ExpectedDir "$relative.code"
        if (-not (Test-Path $expectedTxt) -or -not (Test-Path $expectedCodeFile)) {
            Write-Output "[SEM-ESPERADO] $label"
            $fail++
            continue
        }

        $actual = (& $Jvm -i -cp $testDir.FullName $name 2>$null) -join "`n"
        $code = $LASTEXITCODE
        $expected = ((Get-Content -Raw -Encoding UTF8 $expectedTxt) -replace "`r", "").TrimEnd("`n")
        $expectedCode = [int](Get-Content $expectedCodeFile)

        $sameOutput = ($actual.TrimEnd("`n") -ceq $expected)
        $sameCode = (($expectedCode -eq 0) -eq ($code -eq 0))
        if ($sameOutput -and $sameCode) {
            Write-Output "[OK]     $label"
            $pass++
        } else {
            Write-Output "[FALHOU] $label (codigo esperado $expectedCode, obtido $code)"
            $fail++
        }
    }
}

Write-Output ""
Write-Output "Resultado: $pass ok, $fail falha(s)"
if ($fail -ne 0) { exit 1 }
