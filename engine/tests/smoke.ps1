<#
.SYNOPSIS
    Sends the Phase 1 command sequences to a running engine and checks it survives.

.DESCRIPTION
    Every case here is one of the crashes or hangs the architecture review found.
    The check after each is the same: GET /status still answers. That route only
    reads atomics, so a reply proves the command thread is alive and the state
    machine is still coherent.

    A 2-second 440 Hz stereo tone is generated as the fixture, so the script needs
    no audio file of its own. It is written with an upper-case .WAV extension on
    purpose - the loader used to compare extensions case-sensitively with no else
    branch, and marked the file loaded anyway.

.EXAMPLE
    pwsh engine/tests/smoke.ps1
#>
[CmdletBinding()]
param(
    [string]$EnginePath = (Join-Path $PSScriptRoot '../build/Release/AdagioEngine.exe'),
    [int]$Port = 5000
)

$ErrorActionPreference = 'Stop'
$script:Base = "http://127.0.0.1:$Port"
$script:Failures = @()
$script:Skipped = @()

function New-ToneWav {
    param([string]$Path, [double]$Seconds = 2.0, [int]$SampleRate = 44100, [double]$Frequency = 440.0)

    $channels = 2
    $frames = [int]($Seconds * $SampleRate)
    $dataBytes = $frames * $channels * 2
    $pcm = New-Object byte[] $dataBytes
    for ($i = 0; $i -lt $frames; $i++) {
        $value = [int16](20000 * [Math]::Sin(2 * [Math]::PI * $Frequency * $i / $SampleRate))
        $bytes = [BitConverter]::GetBytes($value)
        $offset = $i * 4
        $pcm[$offset] = $bytes[0]; $pcm[$offset + 1] = $bytes[1]
        $pcm[$offset + 2] = $bytes[0]; $pcm[$offset + 3] = $bytes[1]
    }

    $stream = [System.IO.File]::Create($Path)
    $writer = New-Object System.IO.BinaryWriter($stream)
    $ascii = [System.Text.Encoding]::ASCII
    $writer.Write($ascii.GetBytes('RIFF'))
    $writer.Write([int](36 + $dataBytes))
    $writer.Write($ascii.GetBytes('WAVE'))
    $writer.Write($ascii.GetBytes('fmt '))
    $writer.Write([int]16)
    $writer.Write([int16]1)
    $writer.Write([int16]$channels)
    $writer.Write([int]$SampleRate)
    $writer.Write([int]($SampleRate * $channels * 2))
    $writer.Write([int16]($channels * 2))
    $writer.Write([int16]16)
    $writer.Write($ascii.GetBytes('data'))
    $writer.Write([int]$dataBytes)
    $writer.Write($pcm)
    $writer.Close()
    $stream.Close()
}

function Send-Command {
    param([string]$Route, [string]$Body = '')
    try {
        Invoke-WebRequest -Uri "$script:Base/$Route" -Method Post -Body $Body -ContentType 'text/plain' -TimeoutSec 10 | Out-Null
        return $true
    } catch {
        return $false
    }
}

function Get-Status {
    try {
        $response = Invoke-WebRequest -Uri "$script:Base/status" -Method Get -TimeoutSec 10
        return ($response.Content | ConvertFrom-Json).value
    } catch {
        return $null
    }
}

# Polling on state alone races the command queue: right after a POST the engine is
# still in its old state because the command has not been dequeued yet. Waiting on
# the handled-command counter instead makes every step deterministic.
function Wait-Handled {
    param([long]$Baseline, [int]$Count = 1, [int]$TimeoutMs = 30000)
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $status = Get-Status
        if ($null -ne $status -and $status.commandsHandled -ge ($Baseline + $Count)) { return $status }
        Start-Sleep -Milliseconds 25
    }
    return $null
}

function Get-Handled {
    $status = Get-Status
    if ($null -eq $status) { return -1 }
    return [long]$status.commandsHandled
}

function Invoke-Commands {
    param([object[]]$Commands, [int]$TimeoutMs = 30000)
    $baseline = Get-Handled
    if ($baseline -lt 0) { throw 'engine is not answering' }
    foreach ($command in $Commands) {
        if ($command -is [string]) { Send-Command $command | Out-Null }
        else { Send-Command $command[0] $command[1] | Out-Null }
    }
    $status = Wait-Handled -Baseline $baseline -Count $Commands.Count -TimeoutMs $TimeoutMs
    if ($null -eq $status) { throw "engine did not finish $($Commands.Count) command(s) in $TimeoutMs ms" }
    return $status
}

function Test-Case {
    param([string]$Name, [scriptblock]$Body)

    Write-Host "  $Name ... " -NoNewline
    try {
        & $Body
        $status = Get-Status
        if ($null -eq $status) {
            $script:Failures += "$Name (engine stopped answering)"
            Write-Host 'ENGINE GONE' -ForegroundColor Red
            return
        }
        Write-Host "ok (state=$($status.state))" -ForegroundColor Green
    } catch {
        $script:Failures += "$Name ($($_.Exception.Message))"
        Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# --- fixture -----------------------------------------------------------------
$fixtureDir = Join-Path ([System.IO.Path]::GetTempPath()) "adagio-smoke-$PID"
New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
$tonePath = Join-Path $fixtureDir 'Tone.WAV'
$textPath = Join-Path $fixtureDir 'notaudio.txt'
Write-Host 'Generating fixtures...'
New-ToneWav -Path $tonePath
Set-Content -Path $textPath -Value 'not audio'

if (-not (Test-Path $EnginePath)) {
    Write-Error "Engine not found at $EnginePath. Build it first: cmake --build engine/build --config Release"
    exit 2
}

Write-Host "Starting $EnginePath"
$engine = Start-Process -FilePath $EnginePath -PassThru -WindowStyle Hidden
try {
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline -and $null -eq (Get-Status)) { Start-Sleep -Milliseconds 100 }
    if ($null -eq (Get-Status)) {
        Write-Error 'Engine never answered /status.'
        exit 2
    }

    Write-Host "`nCommands with no file loaded (X5)"
    Test-Case 'play, pause, stop, clear, seek, speed, analysis on an empty engine' {
        $status = Invoke-Commands @('play', 'pause', 'stop', 'clear', 'requestAnalysis',
            @('seek', '1.5'), @('speed', '50'), @('volume', '80'))
        if ($status.state -ne 'empty') { throw "expected state empty, got $($status.state)" }
    }

    Write-Host "`nLoading (X1)"
    Test-Case 'load with an empty path' {
        $status = Invoke-Commands @(, @('load', ''))
        if ($status.state -ne 'empty') { throw "expected state empty, got $($status.state)" }
    }

    Test-Case 'load an unsupported extension' {
        $status = Invoke-Commands @(, @('load', $textPath))
        if ($status.state -ne 'empty') { throw "expected state empty, got $($status.state)" }
    }

    $deviceAvailable = $true
    Test-Case 'load Tone.WAV (upper-case extension)' {
        $status = Invoke-Commands @(, @('load', $tonePath))
        if ($status.state -eq 'empty') {
            $script:deviceAvailable = $false
            $script:Skipped += 'playback cases (no audio output device on this machine)'
        } elseif ($status.duration -lt 1.9 -or $status.duration -gt 2.1) {
            throw "expected a 2 s duration, got $($status.duration)"
        }
    }

    if (-not $deviceAvailable) {
        Write-Host "`nNo output device: skipping the playback cases." -ForegroundColor Yellow
    } else {
        Write-Host "`nRe-entrancy (X2, X3)"
        Test-Case 'two Play commands back to back' {
            $status = Invoke-Commands @('play', 'play')
            if ($status.state -ne 'playing') { throw "expected playing, got $($status.state)" }
        }

        Test-Case 'load while a file is loaded' {
            $status = Invoke-Commands @(@('load', $tonePath), @('load', $tonePath))
            if ($status.state -ne 'ready') { throw "expected ready, got $($status.state)" }
        }

        Write-Host "`nPlayback (X4, T1, T7)"
        Test-Case 'play to the end at 50% speed' {
            Invoke-Commands @(@('speed', '50'), 'play') | Out-Null
            # 2 s of audio at half speed is 4 s of wall clock; allow generous slack.
            Start-Sleep -Seconds 7
            $status = Get-Status
            if ($status.state -ne 'playing') { throw "expected still playing, got $($status.state)" }
            if ($status.position -lt 1.5) { throw "position stalled at $($status.position)" }
        }

        Test-Case 'seek to 0.2 s before the end' {
            Invoke-Commands @(@('speed', '100'), @('seek', '1.8')) | Out-Null
            Start-Sleep -Milliseconds 1500
            Get-Status | Out-Null
        }

        Test-Case 'seek backwards while playing' {
            Invoke-Commands @(, @('seek', '0.2')) | Out-Null
            Start-Sleep -Milliseconds 800
            $status = Get-Status
            if ($status.position -gt 1.5) { throw "seek did not take: position $($status.position)" }
        }

        Write-Host "`nTeardown (X3)"
        Test-Case 'clear while paused' {
            $status = Invoke-Commands @('pause', 'clear')
            if ($status.state -ne 'empty') { throw "expected empty, got $($status.state)" }
        }

        Test-Case 'load, play and clear again after a clear' {
            $status = Invoke-Commands @(, @('load', $tonePath))
            if ($status.state -ne 'ready') { throw 'reload failed' }
            Invoke-Commands @(, 'play') | Out-Null
            Start-Sleep -Milliseconds 400
            $status = Invoke-Commands @(, 'clear')
            if ($status.state -ne 'empty') { throw 'second clear failed' }
        }
    }

    Write-Host "`nShutdown (T9)"
    Send-Command 'shutdown' | Out-Null
    if ($engine.WaitForExit(10000)) {
        Write-Host '  engine exited on request ... ok' -ForegroundColor Green
    } else {
        $script:Failures += 'shutdown (engine did not exit within 10 s)'
        Write-Host '  engine did not exit ... FAILED' -ForegroundColor Red
    }
} finally {
    if (-not $engine.HasExited) { Stop-Process -Id $engine.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -Recurse -Force $fixtureDir -ErrorAction SilentlyContinue
}

Write-Host ''
foreach ($skip in $script:Skipped) { Write-Host "SKIPPED: $skip" -ForegroundColor Yellow }
if ($script:Failures.Count -gt 0) {
    Write-Host "$($script:Failures.Count) failure(s):" -ForegroundColor Red
    foreach ($failure in $script:Failures) { Write-Host "  - $failure" -ForegroundColor Red }
    exit 1
}
Write-Host 'Smoke test passed.' -ForegroundColor Green
exit 0
