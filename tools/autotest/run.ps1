param([string]$config = "Release", [int]$seconds = 20, [string]$mode = "combat")
# 시험 장치를 붙인 빌드를 정해진 시간만 돌리고 끈다. 콘솔 출력은 SimpleGame/Logs/autotest_구성.txt
# 예) powershell -File tools/autotest/run.ps1 -config Release -seconds 8 -mode showcase
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$game = Join-Path $root "SimpleGame"
$exe = Join-Path $root "x64\$config\SimpleGame.exe"
$log = Join-Path $game "Logs\autotest_$config.txt"

New-Item -ItemType Directory -Force (Join-Path $game "Logs") | Out-Null
Remove-Item (Join-Path $game "Logs\cap_*.bmp") -ErrorAction SilentlyContinue

$env:SG_AUTOTEST = $mode
$process = Start-Process -FilePath $exe -WorkingDirectory $game -RedirectStandardOutput $log -PassThru
Start-Sleep -Seconds $seconds
if ($process.HasExited) { "exited code $($process.ExitCode)" } else { Stop-Process -Id $process.Id -Force; "stopped" }
Remove-Item Env:\SG_AUTOTEST

$count = (Get-ChildItem (Join-Path $game "Logs\cap_*.bmp") -ErrorAction SilentlyContinue).Count
"captures $count, log $log"
