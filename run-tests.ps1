# Прогон юнит-тестов проекта под хостом (окружение native из platformio.ini).
#
# Зачем скрипт: ни PlatformIO, ни хостовый компилятор в PATH не лежат, поэтому
# запуск руками каждый раз превращается в две длинные строки. Скрипт находит
# оба инструмента сам, а свои аргументы передает в pio как есть:
#
#   .\run-tests.ps1                     # все наборы
#   .\run-tests.ps1 -f test_ota_image   # один набор
#   .\run-tests.ps1 -vvv                # подробный вывод
#
# Плата не нужна: окружение native собирается обычным gcc и выполняется
# за секунды. Прошивки этот скрипт не трогает — они собираются через pio run

$ErrorActionPreference = 'Stop'

# Работаем из корня проекта, откуда бы скрипт ни запустили
Set-Location -Path $PSScriptRoot

# PlatformIO ставится в собственное виртуальное окружение и в PATH не попадает
$pio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe'

if (-not (Test-Path $pio)) {
    Write-Host "PlatformIO Core не найден: $pio" -ForegroundColor Red
    Write-Host "Поставьте расширение PlatformIO в VS Code или pip install platformio"
    exit 1
}

# Окружению native нужен обычный компилятор для хоста. Если его нет в PATH,
# ищем WinLibs MinGW-w64, поставленный через WinGet
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    $packages = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'

    $bin = Get-ChildItem -Path $packages -Directory -Filter '*WinLibs*' -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'mingw64\bin' } |
        Where-Object { Test-Path (Join-Path $_ 'g++.exe') } |
        Select-Object -First 1

    if (-not $bin) {
        Write-Host "Хостовый компилятор g++ не найден" -ForegroundColor Red
        Write-Host "Поставьте MinGW-w64: winget install BrechtSanders.WinLibs.POSIX.UCRT"
        exit 1
    }

    $env:PATH = "$env:PATH;$bin"
    Write-Host "Компилятор: $bin" -ForegroundColor DarkGray
}

& $pio test -e native @args

exit $LASTEXITCODE
