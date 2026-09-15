<#
.SYNOPSIS
    Sends the Phase 1 command sequences to a running engine and checks it survives.

.DESCRIPTION
    Every case here is one of the crashes or hangs the architecture review found.
    The check after each is the same: GET /status still answers. That route only
    reads atomics, so a reply proves the command thread is alive and the state
    machine is still coherent.

    440 Hz stereo tones are generated as fixtures, so the script needs no audio
    file of its own. The 2-second one is written with an upper-case .WAV extension
    on purpose - the loader used to compare extensions case-sensitively with no
    else branch, and marked the file loaded anyway. The 8-second one outlasts the
    engine's 5-second playback buffer.

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
    param([string]$Path, [int]$Seconds = 2, [int]$SampleRate = 44100, [int]$Frequency = 440)

    $channels = 2
    $bytesPerFrame = $channels * 2
    $dataBytes = $Seconds * $SampleRate * $bytesPerFrame

    # Per-sample PowerShell is slow; a whole-number frequency lets one second tile without clicks.
    $second = New-Object byte[] ($SampleRate * $bytesPerFrame)
    for ($i = 0; $i -lt $SampleRate; $i++) {
        $value = [int16](20000 * [Math]::Sin(2 * [Math]::PI * $Frequency * $i / $SampleRate))
        $bytes = [BitConverter]::GetBytes($value)
        $offset = $i * $bytesPerFrame
        $second[$offset] = $bytes[0]; $second[$offset + 1] = $bytes[1]
        $second[$offset + 2] = $bytes[0]; $second[$offset + 3] = $bytes[1]
    }
    $pcm = New-Object byte[] $dataBytes
    for ($offset = 0; $offset -lt $dataBytes; $offset += $second.Length) {
        [Array]::Copy($second, 0, $pcm, $offset, $second.Length)
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

# One sample can't tell a stalled playhead from a slow machine, so wait for a position instead.
function Wait-Status {
    param([scriptblock]$Until, [int]$TimeoutMs = 5000)
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $status = Get-Status
        if ($null -ne $status -and (& $Until $status)) { return $status }
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
$longTonePath = Join-Path $fixtureDir 'LongTone.wav'
$textPath = Join-Path $fixtureDir 'notaudio.txt'
Write-Host 'Generating fixtures...'
New-ToneWav -Path $tonePath
New-ToneWav -Path $longTonePath -Seconds 8
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

        Write-Host "`nPlayback (X4, T1, T7, T11)"
        Test-Case 'play to the end at 50% speed' {
            Invoke-Commands @(@('speed', '50'), 'play') | Out-Null
            # The playhead reaches the duration only once the stretcher's tail has been flushed.
            $status = Wait-Status { param($s) $s.position -ge $s.duration - 0.001 } 10000
            if ($null -eq $status) { throw "never reached the end: position stopped at $((Get-Status).position)" }
            if ($status.state -ne 'playing') { throw "expected still playing, got $($status.state)" }
        }

        # The feeder has written the whole 2 s track by now, so these seeks start from end of file (T10).
        Test-Case 'seek to 0.2 s before the end' {
            $status = Invoke-Commands @(@('speed', '100'), @('seek', '1.8'))
            if ($status.position -gt 1.95) { throw "seek did not take: position $($status.position)" }
            $status = Wait-Status { param($s) $s.position -ge $s.duration - 0.001 } 3000
            if ($null -eq $status) { throw "stalled after the seek: position $((Get-Status).position)" }
        }

        Test-Case 'seek backwards while playing' {
            $status = Invoke-Commands @(, @('seek', '0.2'))
            if ($status.position -gt 0.5) { throw "seek did not take: position $($status.position)" }
            $status = Wait-Status { param($s) $s.position -ge 0.7 } 3000
            if ($null -eq $status) { throw "stalled after the seek: position $((Get-Status).position)" }
        }

        Write-Host "`nSeeking once the feeder has written the whole track (T10)"
        Test-Case 'seek back from the last 5 s of an 8 s track' {
            # From 6.5 s the feeder reaches end of file at once, leaving most of the buffer free.
            Invoke-Commands @(@('load', $longTonePath), 'play', @('seek', '6.5')) | Out-Null
            if ($null -eq (Wait-Status { param($s) $s.position -ge 6.7 } 3000)) { throw 'playback did not start after seeking to 6.5 s' }

            # Dropping the refilled post-seek audio would run the track out early and jump the playhead to 8 s.
            $status = Invoke-Commands @(, @('seek', '1.0'))
            if ($status.position -gt 1.3) { throw "seek did not take: position $($status.position)" }
            $status = Wait-Status { param($s) $s.position -ge 6.5 } 8000
            if ($null -eq $status) { throw "stalled after seeking back: position $((Get-Status).position)" }
            if ($status.position -gt 7.5) { throw "skipped ahead after seeking back: position $($status.position)" }
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
