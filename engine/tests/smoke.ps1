<#
.SYNOPSIS
    Drives a running engine over its WebSocket and checks it survives.

.DESCRIPTION
    Every case here is one of the crashes or hangs the architecture review found,
    plus the protocol and authentication rules Phase 4 added.

    Commands and events share one socket now, so a command is checked by its reply
    rather than by polling: {id, cmd, args} goes out, {type:"reply", id, ok} comes
    back once the command thread has actually run it. The status command still
    answers from atomics, so it doubles as the liveness probe.

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
    [int]$Port = 9001,
    # A Uint16 spectrum every 64-sample hop (125 Hz) plus the 30 Hz analysis event.
    [double]$ThroughputBudgetKBps = 768
)

$ErrorActionPreference = 'Stop'
$script:Failures = @()
$script:Skipped = @()
$script:NextId = 0
$script:Socket = $null
# Non-reply frames seen while waiting for a reply, so a case can assert on the
# events a command produced as well as on its answer.
$script:Events = @()

# The engine is started with a token, so the script also proves that a client
# without it is refused.
$script:Token = [guid]::NewGuid().ToString('N')

# A .NET reader thread rather than a PowerShell loop: during playback the engine
# sends a spectrum frame every hop and a position event at 30 Hz, and those are
# counted or dropped here instead of being handed to PowerShell to parse. Binary
# frames are never text, so they are counted by kind and never queued.
Add-Type -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text;
using System.Threading;

public class AdagioSocket
{
    private ClientWebSocket _socket;
    private CancellationTokenSource _cancel;
    private ConcurrentQueue<string> _messages = new ConcurrentQueue<string>();
    private Thread _reader;
    private string _closeReason;
    private int _spectrumFrames;
    private int _waveformFrames;
    private int _analysisEvents;
    private int _endOfPlayEvents;
    private int _positionEvents;

    public int SpectrumFrames { get { return Volatile.Read(ref _spectrumFrames); } }
    public int WaveformFrames { get { return Volatile.Read(ref _waveformFrames); } }
    public int AnalysisEvents { get { return Volatile.Read(ref _analysisEvents); } }
    public int EndOfPlayEvents { get { return Volatile.Read(ref _endOfPlayEvents); } }
    public int PositionEvents { get { return Volatile.Read(ref _positionEvents); } }
    public void ResetCounts()
    {
        Interlocked.Exchange(ref _spectrumFrames, 0);
        Interlocked.Exchange(ref _waveformFrames, 0);
        Interlocked.Exchange(ref _analysisEvents, 0);
        Interlocked.Exchange(ref _endOfPlayEvents, 0);
        Interlocked.Exchange(ref _positionEvents, 0);
    }

    public string CloseReason { get { return _closeReason; } }
    public bool IsOpen { get { return _socket != null && _socket.State == WebSocketState.Open; } }

    public bool Connect(string url, string origin, int timeoutMs)
    {
        _socket = new ClientWebSocket();
        if (!string.IsNullOrEmpty(origin))
            _socket.Options.SetRequestHeader("Origin", origin);
        _cancel = new CancellationTokenSource();
        try
        {
            if (!_socket.ConnectAsync(new Uri(url), _cancel.Token).Wait(timeoutMs))
                return false;
        }
        catch (Exception e)
        {
            _closeReason = e.GetBaseException().Message;
            return false;
        }

        _reader = new Thread(ReadLoop);
        _reader.IsBackground = true;
        _reader.Start();
        return true;
    }

    private void ReadLoop()
    {
        byte[] buffer = new byte[65536];
        StringBuilder text = new StringBuilder();
        try
        {
            while (_socket.State == WebSocketState.Open)
            {
                text.Length = 0;
                WebSocketReceiveResult result;
                bool binary = false;
                int kind = -1;
                do
                {
                    var task = _socket.ReceiveAsync(new ArraySegment<byte>(buffer), _cancel.Token);
                    task.Wait();
                    result = task.Result;
                    if (result.MessageType == WebSocketMessageType.Close)
                    {
                        _closeReason = result.CloseStatusDescription;
                        return;
                    }
                    if (result.MessageType == WebSocketMessageType.Binary)
                    {
                        // The kind is the frame's first byte, so only the first fragment matters.
                        if (!binary && result.Count > 0) kind = buffer[0];
                        binary = true;
                        continue;
                    }
                    text.Append(Encoding.UTF8.GetString(buffer, 0, result.Count));
                }
                while (!result.EndOfMessage);

                if (binary)
                {
                    if (kind == 1) Interlocked.Increment(ref _spectrumFrames);
                    else if (kind == 2) Interlocked.Increment(ref _waveformFrames);
                    continue;
                }

                string message = text.ToString();
                if (message.StartsWith("{\"type\":\"position\""))
                {
                    Interlocked.Increment(ref _positionEvents);
                    continue;
                }
                if (message.StartsWith("{\"type\":\"analysis\""))
                    Interlocked.Increment(ref _analysisEvents);
                else if (message.StartsWith("{\"type\":\"endOfPlay\""))
                    Interlocked.Increment(ref _endOfPlayEvents);
                _messages.Enqueue(message);
            }
        }
        catch (Exception e)
        {
            if (_closeReason == null)
                _closeReason = e.GetBaseException().Message;
        }
    }

    public void Send(string text)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(text);
        _socket.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, _cancel.Token).Wait(5000);
    }

    public string Take()
    {
        string message;
        if (_messages.TryDequeue(out message))
            return message;
        return null;
    }

    // Returns the reason the server gave for closing, or null while it stays open.
    public string WaitClosed(int timeoutMs)
    {
        DateTime deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
        while (DateTime.UtcNow < deadline)
        {
            if (_socket.State != WebSocketState.Open)
                return _closeReason != null ? _closeReason : _socket.State.ToString();
            Thread.Sleep(25);
        }
        return null;
    }

    public void Close()
    {
        try { if (_cancel != null) _cancel.Cancel(); } catch { }
        try { if (_socket != null) _socket.Abort(); } catch { }
    }
}
'@

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

function Connect-Engine {
    param([string]$Token = $script:Token, [string]$Origin = '', [int]$TimeoutMs = 5000)

    $url = "ws://127.0.0.1:$Port/"
    if ($Token) { $url += "?token=$Token" }

    $socket = New-Object AdagioSocket
    if (-not $socket.Connect($url, $Origin, $TimeoutMs)) { return $null }
    return $socket
}

# Sends one command and waits for the reply that carries its id. Anything else that
# arrives meanwhile is kept for the assertions rather than thrown away.
function Invoke-EngineCommand {
    param([string]$Cmd, $Arguments = $null, [int]$TimeoutMs = 30000)

    $script:NextId++
    $id = [string]$script:NextId
    $payload = [ordered]@{ id = $id; cmd = $Cmd }
    if ($null -ne $Arguments) { $payload['args'] = $Arguments }
    $script:Socket.Send(($payload | ConvertTo-Json -Compress))

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $raw = $script:Socket.Take()
        if ($null -eq $raw) {
            if (-not $script:Socket.IsOpen) { throw "the engine closed the connection ($($script:Socket.CloseReason))" }
            Start-Sleep -Milliseconds 5
            continue
        }
        $msg = $raw | ConvertFrom-Json
        if ($msg.type -eq 'reply' -and $msg.id -eq $id) { return $msg }
        $script:Events += $msg
        if ($script:Events.Count -gt 200) { $script:Events = $script:Events[-100..-1] }
    }
    throw "no reply to '$Cmd' within $TimeoutMs ms"
}

# Fire and forget, for the one command that cannot answer: by the time the reply
# would be broadcast the engine is already tearing the socket down.
function Send-EngineCommand {
    param([string]$Cmd)
    $script:Socket.Send((@{ cmd = $Cmd } | ConvertTo-Json -Compress))
}

function Get-Status {
    try {
        return (Invoke-EngineCommand -Cmd 'status' -TimeoutMs 10000).value
    } catch {
        return $null
    }
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

# Every command is acknowledged, so a sequence is simply awaited: when the last reply
# is in, every one of them has been through the command thread.
function Invoke-Commands {
    param([object[]]$Commands, [int]$TimeoutMs = 30000)
    foreach ($command in $Commands) {
        if ($command -is [string]) { Invoke-EngineCommand -Cmd $command -TimeoutMs $TimeoutMs | Out-Null }
        else { Invoke-EngineCommand -Cmd $command[0] -Arguments $command[1] -TimeoutMs $TimeoutMs | Out-Null }
    }
    return Get-Status
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
$engine = Start-Process -FilePath $EnginePath -ArgumentList "--token=$script:Token" -PassThru -WindowStyle Hidden
try {
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline -and $null -eq $script:Socket) {
        $script:Socket = Connect-Engine
        if ($null -eq $script:Socket) { Start-Sleep -Milliseconds 200 }
    }
    if ($null -eq $script:Socket) {
        Write-Error 'Engine never accepted a WebSocket connection.'
        exit 2
    }
    if ($null -eq (Get-Status)) {
        Write-Error 'Engine never answered the status command.'
        exit 2
    }

    Write-Host "`nAuthentication (S1)"
    Test-Case 'a client with no token is refused' {
        $probe = Connect-Engine -Token ''
        if ($null -eq $probe) { throw 'could not reach the engine at all' }
        try {
            $reason = $probe.WaitClosed(3000)
            if ($null -eq $reason) { throw 'the engine kept an unauthenticated client' }
        } finally { $probe.Close() }
    }

    Test-Case 'a client with the wrong token is refused' {
        $probe = Connect-Engine -Token 'not-the-token'
        if ($null -eq $probe) { throw 'could not reach the engine at all' }
        try {
            $reason = $probe.WaitClosed(3000)
            if ($null -eq $reason) { throw 'the engine kept a client with a bad token' }
        } finally { $probe.Close() }
    }

    Test-Case 'a page in a browser is refused even with the token' {
        # A browser sets Origin on the handshake and cannot suppress it, so this is
        # exactly what new WebSocket('ws://127.0.0.1:9001') from a website looks like.
        $probe = Connect-Engine -Origin 'https://example.com'
        if ($null -eq $probe) { throw 'could not reach the engine at all' }
        try {
            $reason = $probe.WaitClosed(3000)
            if ($null -eq $reason) { throw 'the engine kept a client from a foreign origin' }
        } finally { $probe.Close() }
    }

    Test-Case "the app's own origin is accepted" {
        $probe = Connect-Engine -Origin 'file://'
        if ($null -eq $probe) { throw 'could not reach the engine at all' }
        try {
            $reason = $probe.WaitClosed(1000)
            if ($null -ne $reason) { throw "the engine refused its own renderer: $reason" }
        } finally { $probe.Close() }
    }

    Write-Host "`nProtocol (S2, S4)"
    Test-Case 'an unknown command is refused by name' {
        $reply = Invoke-EngineCommand -Cmd 'selfDestruct'
        if ($reply.ok) { throw 'the engine accepted a command it does not have' }
        if ($reply.error -notlike '*selfDestruct*') { throw "unhelpful error: $($reply.error)" }
    }

    Test-Case 'a command with the wrong argument type is refused' {
        $reply = Invoke-EngineCommand -Cmd 'seek' -Arguments 'halfway'
        if ($reply.ok) { throw 'the engine accepted a seek to a string' }
    }

    Test-Case 'a state change is broadcast as a transport event' {
        $script:Events = @()
        Invoke-Commands @(, @('setVolume', 0.0)) | Out-Null
        if (-not ($script:Events | Where-Object { $_.type -eq 'transport' })) {
            throw 'no transport event followed a command that changed state'
        }
    }

    Write-Host "`nAnalysis settings"
    Test-Case 'the schema answers before a file is loaded' {
        $reply = Invoke-EngineCommand -Cmd 'getAnalysisSchema'
        if (-not $reply.ok) { throw "schema refused: $($reply.error)" }
        if ($reply.value.stages.Count -lt 7) { throw "expected the whole pipeline, got $($reply.value.stages.Count) stages" }
    }

    Test-Case 'a setting is validated against its own schema' {
        $bad = Invoke-EngineCommand -Cmd 'setAnalysisSetting' -Arguments @{ stage = 'FFTProcessor'; key = 'WINDOW'; value = 'Gaussian' }
        if ($bad.ok) { throw 'the engine accepted a window function it does not have' }

        $good = Invoke-EngineCommand -Cmd 'setAnalysisSetting' -Arguments @{ stage = 'FFTProcessor'; key = 'WINDOW'; value = 'Hann' }
        if (-not $good.ok) { throw "a valid setting was refused: $($good.error)" }

        $schema = (Invoke-EngineCommand -Cmd 'getAnalysisSchema').value
        $window = $schema.stages | Where-Object { $_.name -eq 'FFTProcessor' } | ForEach-Object { $_.settings } | Where-Object { $_.key -eq 'WINDOW' }
        if ($window.value -ne 'Hann') { throw "the schema still reports $($window.value)" }
    }

    Write-Host "`nCommands with no file loaded (X5)"
    Test-Case 'play, pause, stop, clear, seek, speed, analysis on an empty engine' {
        $status = Invoke-Commands @('play', 'pause', 'stop', 'clear', 'analyseFrame',
            @('seek', 1.5), @('setSpeed', 0.5), @('setVolume', 0.8))
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
        # Nothing here needs to be heard, and a bench running the suite should not have to listen to it.
        Invoke-Commands @(, @('setVolume', 0.0)) | Out-Null

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
        # The engine ends the track itself: once the source is exhausted and the stretcher's
        # tail has been flushed, it sends endOfPlay once and stops, which rewinds to 0.
        Test-Case 'play to the end at 50% speed ends the track once' {
            $script:Socket.ResetCounts()
            $started = Get-Date
            Invoke-Commands @(@('setSpeed', 0.5), 'play') | Out-Null
            $status = Wait-Status { param($s) $s.state -eq 'ready' } 10000
            if ($null -eq $status) { $s = Get-Status; throw "never ended: $($s.state) at $($s.position)" }
            # 2 s at 50% is 4 s of audio. Ending much sooner means the tail was cut off.
            $elapsed = ((Get-Date) - $started).TotalSeconds
            if ($elapsed -lt 3.5) { throw ("ended after {0:N1} s, expected about 4" -f $elapsed) }
            if ($status.position -ne 0) { throw "expected a stopped track at 0, got $($status.position)" }
            Start-Sleep -Milliseconds 300
            if ($script:Socket.EndOfPlayEvents -ne 1) { throw "expected one endOfPlay, got $($script:Socket.EndOfPlayEvents)" }
        }

        Test-Case 'seek to 0.2 s before the end, then the track ends at 100%' {
            $script:Socket.ResetCounts()
            $status = Invoke-Commands @(@('setSpeed', 1.0), 'play', @('seek', 1.8))
            if ($status.position -gt 1.95) { throw "seek did not take: position $($status.position)" }
            $status = Wait-Status { param($s) $s.state -eq 'ready' } 3000
            if ($null -eq $status) { $s = Get-Status; throw "stalled after the seek: $($s.state) at $($s.position)" }
            Start-Sleep -Milliseconds 300
            if ($script:Socket.EndOfPlayEvents -ne 1) { throw "expected one endOfPlay, got $($script:Socket.EndOfPlayEvents)" }
        }

        Test-Case 'seek backwards while playing' {
            Invoke-Commands @('play', @('seek', 1.5)) | Out-Null
            if ($null -eq (Wait-Status { param($s) $s.position -ge 1.6 } 3000)) { throw 'playback did not start after seeking to 1.5 s' }
            $status = Invoke-Commands @(, @('seek', 0.2))
            if ($status.position -gt 0.5) { throw "seek did not take: position $($status.position)" }
            $status = Wait-Status { param($s) $s.position -ge 0.7 } 3000
            if ($null -eq $status) { throw "stalled after the seek: position $((Get-Status).position)" }
        }

        Write-Host "`nSeeking once the feeder has written the whole track (T10)"
        Test-Case 'seek back from the last 5 s of an 8 s track' {
            # From 6.5 s the feeder reaches end of file at once, leaving most of the buffer free.
            Invoke-Commands @(@('load', $longTonePath), 'play', @('seek', 6.5)) | Out-Null
            if ($null -eq (Wait-Status { param($s) $s.position -ge 6.7 } 3000)) { throw 'playback did not start after seeking to 6.5 s' }

            # Dropping the refilled post-seek audio would run the track out early and jump the playhead to 8 s.
            $status = Invoke-Commands @(, @('seek', 1.0))
            if ($status.position -gt 1.3) { throw "seek did not take: position $($status.position)" }
            $status = Wait-Status { param($s) $s.position -ge 6.5 } 8000
            if ($null -eq $status) { throw "stalled after seeking back: position $((Get-Status).position)" }
            if ($status.position -gt 7.5) { throw "skipped ahead after seeking back: position $($status.position)" }
        }

        Write-Host "`nBinary frames (S4, F4)"
        Test-Case 'five waveform frames arrive before the load is answered' {
            $script:Socket.ResetCounts()
            # They are queued ahead of fileLoaded and the reply, so the count is exact by now.
            $status = Invoke-Commands @(, @('load', $longTonePath))
            if ($status.state -ne 'ready') { throw "expected ready, got $($status.state)" }
            if ($script:Socket.WaveformFrames -ne 5) { throw "expected 5 waveform frames, got $($script:Socket.WaveformFrames)" }
        }

        Test-Case 'playback streams spectrum frames and a 30 Hz analysis event without a spectrum' {
            $script:Events = @()
            $script:Socket.ResetCounts()
            Invoke-Commands @(@('setSpeed', 1.0), 'play') | Out-Null
            Start-Sleep -Milliseconds 2000
            Get-Status | Out-Null
            # One frame per 64-sample hop of the 8 kHz analysis stream: 125 a second at 100%.
            $spectrum = $script:Socket.SpectrumFrames
            if ($spectrum -lt 175 -or $spectrum -gt 275) { throw "expected about 250 spectrum frames in 2 s, got $spectrum" }
            $analysis = $script:Socket.AnalysisEvents
            # One per 4 spectrum frames: 31.25 Hz.
            $expected = $spectrum / 4
            if ($analysis -lt $expected - 3 -or $analysis -gt $expected + 3) { throw "expected one analysis event per 4 spectrum frames ($expected), got $analysis" }
            if ($analysis -lt 58) { throw "expected at least 58 analysis events in 2 s, got $analysis" }
            $event = $script:Events | Where-Object { $_.type -eq 'analysis' } | Select-Object -First 1
            if ($null -eq $event) { throw 'no analysis event reached the reader' }
            if ($null -ne $event.value.magnitudes) { throw 'the analysis event still carries the spectrum' }
        }

        # Position comes from the command thread's pump, not the audio callback (T4),
        # so its rate is fixed whatever the device period is.
        Test-Case 'position arrives at about 30 Hz' {
            $script:Socket.ResetCounts()
            Start-Sleep -Milliseconds 2000
            Get-Status | Out-Null
            $positions = $script:Socket.PositionEvents
            if ($positions -lt 50 -or $positions -gt 70) { throw "expected about 60 position events in 2 s, got $positions" }
        }

        Test-Case "throughput stays under $ThroughputBudgetKBps KB/s during playback" {
            # Back to the start, so the 8 s track outlasts the measurement.
            Invoke-Commands @(, @('seek', 0.0)) | Out-Null
            $before = (Get-Status).bytesSent
            Start-Sleep -Milliseconds 5000
            $status = Get-Status
            if ($status.state -ne 'playing') { throw "playback ended early: $($status.state)" }
            $rate = ($status.bytesSent - $before) / 5.0 / 1024.0
            Write-Host ("{0:N1} KB/s " -f $rate) -NoNewline
            if ($rate -gt $ThroughputBudgetKBps) { throw ("{0:N1} KB/s is over the {1} KB/s budget" -f $rate, $ThroughputBudgetKBps) }
        }

        # underrunCount once compared source frames consumed with output frames
        # requested, so at 50% every callback counted.
        Test-Case 'no underruns while playing at 50% speed' {
            Invoke-Commands @(@('seek', 0.0), @('setSpeed', 0.5)) | Out-Null
            # Past the speed change, which resets the stretcher.
            Start-Sleep -Milliseconds 300
            $before = (Get-Status).underrunCount
            Start-Sleep -Milliseconds 3000
            $status = Get-Status
            if ($status.state -ne 'playing') { throw "playback ended early: $($status.state)" }
            $underruns = $status.underrunCount - $before
            if ($underruns -ne 0) { throw "expected no underruns in 3 s, got $underruns" }
            Invoke-Commands @(, @('setSpeed', 1.0)) | Out-Null
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
    Send-EngineCommand 'shutdown'
    if ($engine.WaitForExit(10000)) {
        Write-Host '  engine exited on request ... ok' -ForegroundColor Green
    } else {
        $script:Failures += 'shutdown (engine did not exit within 10 s)'
        Write-Host '  engine did not exit ... FAILED' -ForegroundColor Red
    }
} finally {
    if ($null -ne $script:Socket) { $script:Socket.Close() }
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
