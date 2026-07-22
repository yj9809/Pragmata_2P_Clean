[CmdletBinding()]
param(
    [string]$EngineRoot = "C:\Program Files\Epic Games\UE_5.8",
    [string]$Target = "Pragmata_2PEditor",
    [string]$Platform = "Win64",
    [string]$Configuration = "Development",
    [string]$ProjectPath
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..\..\..")

if (-not $ProjectPath) {
    $ProjectPath = Join-Path $RepoRoot "Pragmata_2P.uproject"
}

$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path -LiteralPath $BuildBat)) {
    throw "Build.bat not found at $BuildBat. Pass -EngineRoot with the Unreal Engine 5.8 install path."
}

if (-not (Test-Path -LiteralPath $ProjectPath)) {
    throw "Project file not found at $ProjectPath."
}

$ResolvedProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectPath)

$LogDir = Join-Path $RepoRoot ".agents\work\build-fix"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$LogPath = Join-Path $LogDir "$Target-$Platform-$Configuration-$Timestamp.log"

function Write-BuildFixLog {
    param([string]$Message)

    Write-Host $Message
    Add-Content -LiteralPath $LogPath -Value $Message -Encoding UTF8
}

function Stop-UnrealEditorProcesses {
    param(
        [array]$ProcessesToStop,
        [bool]$FallbackAllEditors
    )

    foreach ($EditorProcess in $ProcessesToStop) {
        $ProcessId = [int]$EditorProcess.ProcessId
        $Process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
        if (-not $Process) {
            Write-BuildFixLog "UnrealEditor PID $ProcessId is already closed."
            continue
        }

        $Mode = if ($FallbackAllEditors) { "fallback-all" } else { "project-match" }
        Write-BuildFixLog "Closing UnrealEditor PID $ProcessId before build ($Mode)."

        $CloseRequested = $false
        if ($Process.MainWindowHandle -ne 0) {
            $CloseRequested = $Process.CloseMainWindow()
            if ($CloseRequested) {
                [void]$Process.WaitForExit(5000)
            }
        }

        $Process.Refresh()
        if (-not $Process.HasExited) {
            Write-BuildFixLog "Force stopping UnrealEditor PID $ProcessId."
            Stop-Process -Id $ProcessId -Force -ErrorAction Stop
            Wait-Process -Id $ProcessId -Timeout 10 -ErrorAction SilentlyContinue
        }
    }
}

Write-BuildFixLog "[WARNING] This script closes Unreal Editor before build. Unsaved Editor asset changes may be lost."

$AllEditorProcesses = @(
    Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" -ErrorAction SilentlyContinue
)
$ProjectEditorProcesses = @(
    $AllEditorProcesses | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_.CommandLine) -and
        ($_.CommandLine -like "*$ResolvedProjectPath*" -or $_.CommandLine -like "*$ProjectName*")
    }
)
$UnidentifiedEditorProcesses = @(
    $AllEditorProcesses | Where-Object {
        [string]::IsNullOrWhiteSpace($_.CommandLine)
    }
)

if ($ProjectEditorProcesses.Count -gt 0) {
    Write-BuildFixLog "Found $($ProjectEditorProcesses.Count) UnrealEditor process(es) for $ProjectName."
    Stop-UnrealEditorProcesses -ProcessesToStop $ProjectEditorProcesses -FallbackAllEditors $false
} elseif ($AllEditorProcesses.Count -gt 0 -and $UnidentifiedEditorProcesses.Count -gt 0) {
    Write-BuildFixLog "Unable to identify all UnrealEditor command lines; closing all UnrealEditor processes."
    Stop-UnrealEditorProcesses -ProcessesToStop $AllEditorProcesses -FallbackAllEditors $true
} elseif ($AllEditorProcesses.Count -gt 0) {
    Write-BuildFixLog "UnrealEditor is running, but no process matches $ProjectName. Leaving other projects open."
} else {
    Write-BuildFixLog "No UnrealEditor process is running."
}

Write-Host "Build command:"
Write-Host "`"$BuildBat`" $Target $Platform $Configuration -Project=`"$ProjectPath`" -WaitMutex -NoHotReloadFromIDE"
Write-Host "Build log: $LogPath"

& $BuildBat $Target $Platform $Configuration "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE 2>&1 |
    Tee-Object -FilePath $LogPath -Append

$ExitCode = $LASTEXITCODE
Write-Host "Build exit code: $ExitCode"
Write-Host "Build log: $LogPath"
exit $ExitCode
