<#
.SYNOPSIS
    Hammers load / play / clear on a long track and reports the first command the
    engine fails to answer.

.DESCRIPTION
    This exists for one failure: the engine going deaf. A track of a few minutes
    produces a waveform event of several hundred KB, which the WebSocket library
    writes to the socket in 32 KB fragments from the queue thread while the
    connection's own thread is polling. A wake-up missed in that window used to
    park the connection permanently - the socket stayed open, the engine stayed
    healthy, events kept being queued, and nothing the client sent was ever read
    again. Every command after that went unanswered.

    Two things make it reproduce, and smoke.ps1 has neither:

      * a track long enough for the waveform to need many fragments (three
        minutes does it; forty seconds does not), and
      * the next command sent the instant the load is answered. Any round trip in
        between lets the socket settle and the stall does not happen.

    It is timing-dependent, so it is a stress script rather than a smoke case: it
    reproduced on the first cycle every time against a broken build, but one clean
    cycle does not prove much. Run several.

.EXAMPLE
    pwsh engine/tests/stress.ps1
    pwsh engine/tests/stress.ps1 -Cycles 20 -Minutes 6
#>
[CmdletBinding()]
param(
    [string]$EnginePath = (Join-Path $PSScriptRoot '../build/Release/AdagioEngine.exe'),
    [int]$Cycles = 5,
    [int]$Minutes = 3,
    [int]$PlayMs = 1500,
    [int]$ReplyTimeoutMs = 15000,
    [int]$Port = 9001
)

$ErrorActionPreference = 'Stop'
$script:NextId = 0

Add-Type -TypeDefinition @'
using System;
using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text;
using System.Threading;

public class StressSocket
{
    private ClientWebSocket _socket;
    private CancellationTokenSource _cancel;
    private ConcurrentQueue<string> _replies = new ConcurrentQueue<string>();
    public string CloseReason;
    public long Frames;
    public long Bytes;

    public bool IsOpen { get { return _socket != null && _socket.State == WebSocketState.Open; } }

    public bool Connect(string url, int timeoutMs)
    {
        _socket = new ClientWebSocket();
        _cancel = new CancellationTokenSource();
        try { if (!_socket.ConnectAsync(new Uri(url), _cancel.Token).Wait(timeoutMs)) return false; }
        catch (Exception e) { CloseReason = e.GetBaseException().Message; return false; }
        Thread reader = new Thread(ReadLoop);
        reader.IsBackground = true;
        reader.Start();
        return true;
    }

    // Everything is drained and only replies are kept, so a slow client can never be
    // mistaken for the stall this script is looking for.
    private void ReadLoop()
    {
        byte[] buffer = new byte[262144];
        StringBuilder text = new StringBuilder();
        try
        {
            while (_socket.State == WebSocketState.Open)
            {
                text.Length = 0;
                WebSocketReceiveResult result;
                bool binary = false;
                long binaryBytes = 0;
                do
                {
                    var task = _socket.ReceiveAsync(new ArraySegment<byte>(buffer), _cancel.Token);
                    task.Wait();
                    result = task.Result;
                    if (result.MessageType == WebSocketMessageType.Close) { CloseReason = result.CloseStatusDescription; return; }
                    // Spectrum and waveform frames are binary: counted, never decoded as text.
                    if (result.MessageType == WebSocketMessageType.Binary) { binary = true; binaryBytes += result.Count; continue; }
                    text.Append(Encoding.UTF8.GetString(buffer, 0, result.Count));
                } while (!result.EndOfMessage);

                Interlocked.Increment(ref Frames);
                if (binary) { Interlocked.Add(ref Bytes, binaryBytes); continue; }

                string message = text.ToString();
                Interlocked.Add(ref Bytes, message.Length);
                if (message.Contains("\"type\":\"reply\"")) _replies.Enqueue(message);
            }
        }
        catch (Exception e) { if (CloseReason == null) CloseReason = e.GetBaseException().Message; }
    }

    public void Send(string text)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(text);
        _socket.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Text, true, _cancel.Token).Wait(5000);
    }

    public string TakeReply() { string m; if (_replies.TryDequeue(out m)) return m; return null; }
    public void Close() { try { _cancel.Cancel(); _socket.Abort(); } catch { } }
}
'@

function New-ToneWav {
    param([string]$Path, [int]$Seconds, [int]$SampleRate = 44100, [int]$Frequency = 440)

    $channels = 2
    $bytesPerFrame = $channels * 2
    $dataBytes = $Seconds * $SampleRate * $bytesPerFrame

    $second = New-Object byte[] ($SampleRate * $bytesPerFrame)
    for ($i = 0; $i -lt $SampleRate; $i++) {
        $value = [int16](20000 * [Math]::Sin(2 * [Math]::PI * $Frequency * $i / $SampleRate))
        $bytes = [BitConverter]::GetBytes($value)
        $offset = $i * $bytesPerFrame
        $second[$offset] = $bytes[0]; $second[$offset + 1] = $bytes[1]
        $second[$offset + 2] = $bytes[0]; $second[$offset + 3] = $bytes[1]
    }

    $stream = [System.IO.File]::Create($Path)
    $writer = New-Object System.IO.BinaryWriter($stream)
    $ascii = [System.Text.Encoding]::ASCII
    $writer.Write($ascii.GetBytes('RIFF')); $writer.Write([int](36 + $dataBytes))
    $writer.Write($ascii.GetBytes('WAVE')); $writer.Write($ascii.GetBytes('fmt '))
    $writer.Write([int]16); $writer.Write([int16]1); $writer.Write([int16]$channels)
    $writer.Write([int]$SampleRate); $writer.Write([int]($SampleRate * $channels * 2))
    $writer.Write([int16]($channels * 2)); $writer.Write([int16]16)
    $writer.Write($ascii.GetBytes('data')); $writer.Write([int]$dataBytes)
    for ($offset = 0; $offset -lt $dataBytes; $offset += $second.Length) {
        $writer.Write($second, 0, [Math]::Min($second.Length, $dataBytes - $offset))
    }
    $writer.Close(); $stream.Close()
}

# Sends one command and waits for the reply carrying its id. A command that is never
# answered is the failure this script exists to catch, so the timeout throws.
function Invoke-Cmd {
    param([string]$Cmd, $Arguments = $null, [int]$TimeoutMs = $ReplyTimeoutMs)

    $script:NextId++
    $wanted = [string]$script:NextId
    $payload = [ordered]@{ id = $wanted; cmd = $Cmd }
    if ($null -ne $Arguments) { $payload['args'] = $Arguments }

    $started = Get-Date
    $socket.Send(($payload | ConvertTo-Json -Compress))

    $deadline = $started.AddMilliseconds($TimeoutMs)
    while ((Get-Date) -lt $deadline) {
        $raw = $socket.TakeReply()
        if ($null -eq $raw) {
            if (-not $socket.IsOpen) { throw "socket closed ($($socket.CloseReason))" }
            Start-Sleep -Milliseconds 5
            continue
        }
        $msg = $raw | ConvertFrom-Json
        if ($msg.id -eq $wanted) {
            return [pscustomobject]@{ Ok = $msg.ok; Ms = [int]((Get-Date) - $started).TotalMilliseconds; Value = $msg.value; Error = $msg.error }
        }
    }
    throw "no reply to '$Cmd' in $TimeoutMs ms - the engine stopped reading this connection"
}

$fixtureDir = Join-Path ([System.IO.Path]::GetTempPath()) "adagio-stress-$PID"
New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null
$tonePath = Join-Path $fixtureDir 'LongTone.wav'
Write-Host "Generating a $Minutes-minute fixture..."
New-ToneWav -Path $tonePath -Seconds ($Minutes * 60)

if (-not (Test-Path $EnginePath)) {
    Write-Error "Engine not found at $EnginePath. Build it first: cmake --build engine/build --config Release"
    exit 2
}

Write-Host "Starting $EnginePath"
$engine = Start-Process -FilePath $EnginePath -PassThru -WindowStyle Hidden
$socket = $null
try {
    $deadline = (Get-Date).AddSeconds(15)
    while ((Get-Date) -lt $deadline -and $null -eq $socket) {
        $candidate = New-Object StressSocket
        if ($candidate.Connect("ws://127.0.0.1:$Port/", 3000)) { $socket = $candidate } else { Start-Sleep -Milliseconds 200 }
    }
    if ($null -eq $socket) { Write-Error 'Engine never accepted a WebSocket connection.'; exit 2 }

    # Nothing here needs to be heard.
    Invoke-Cmd -Cmd 'setVolume' -Arguments 0.0 | Out-Null

    $probe = Invoke-Cmd -Cmd 'load' -Arguments $tonePath -TimeoutMs 60000
    if (-not $probe.Ok) {
        Write-Host "SKIPPED: the engine cannot load audio here ($($probe.Error))" -ForegroundColor Yellow
        Write-Host 'This usually means the machine has no audio output device.' -ForegroundColor Yellow
        exit 0
    }
    Invoke-Cmd -Cmd 'clear' | Out-Null

    for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
        Write-Host "cycle $cycle"
        $r = Invoke-Cmd -Cmd 'load' -Arguments $tonePath -TimeoutMs 60000
        Write-Host ("  load   {0,6} ms" -f $r.Ms)

        # Straight after the load's reply, with no round trip in between.
        $r = Invoke-Cmd -Cmd 'play'
        Write-Host ("  play   {0,6} ms" -f $r.Ms)

        Start-Sleep -Milliseconds $PlayMs
        $r = Invoke-Cmd -Cmd 'status'
        Write-Host ("  status {0,6} ms  state={1} pos={2:N2}" -f $r.Ms, $r.Value.state, $r.Value.position)

        $r = Invoke-Cmd -Cmd 'clear'
        Write-Host ("  clear  {0,6} ms" -f $r.Ms)
    }

    Write-Host ("Survived {0} cycles, {1:N0} frames, {2:N0} bytes." -f $Cycles, $socket.Frames, $socket.Bytes) -ForegroundColor Green
} catch {
    Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    if ($null -ne $socket) { $socket.Close() }
    if (-not $engine.HasExited) { Stop-Process -Id $engine.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -Recurse -Force $fixtureDir -ErrorAction SilentlyContinue
}
exit 0
