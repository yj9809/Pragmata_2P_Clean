param(
    [string]$DocsPath = "Docs/Operations/unstaged-assets.md",
    [string]$UploadRoot = "gd-upload",
    [string]$ZipName = "unstaged-assets-content.zip"
)

$ErrorActionPreference = "Stop"

function Get-LockingProcesses {
    param([string]$Path)

    $source = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class RestartManagerLockCheck {
  const int RmRebootReasonNone = 0;
  const int CCH_RM_MAX_APP_NAME = 255;
  const int CCH_RM_MAX_SVC_NAME = 63;
  enum RM_APP_TYPE { RmUnknownApp = 0, RmMainWindow = 1, RmOtherWindow = 2, RmService = 3, RmExplorer = 4, RmConsole = 5, RmCritical = 1000 }
  [StructLayout(LayoutKind.Sequential)] struct RM_UNIQUE_PROCESS { public int dwProcessId; public System.Runtime.InteropServices.ComTypes.FILETIME ProcessStartTime; }
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct RM_PROCESS_INFO {
    public RM_UNIQUE_PROCESS Process;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCH_RM_MAX_APP_NAME + 1)] public string strAppName;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCH_RM_MAX_SVC_NAME + 1)] public string strServiceShortName;
    public RM_APP_TYPE ApplicationType;
    public uint AppStatus;
    public uint TSSessionId;
    [MarshalAs(UnmanagedType.Bool)] public bool bRestartable;
  }
  [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)] static extern int RmStartSession(out uint pSessionHandle, int dwSessionFlags, string strSessionKey);
  [DllImport("rstrtmgr.dll", CharSet = CharSet.Unicode)] static extern int RmRegisterResources(uint pSessionHandle, uint nFiles, string[] rgsFilenames, uint nApplications, IntPtr rgApplications, uint nServices, string[] rgsServiceNames);
  [DllImport("rstrtmgr.dll")] static extern int RmGetList(uint dwSessionHandle, out uint pnProcInfoNeeded, ref uint pnProcInfo, [In, Out] RM_PROCESS_INFO[] rgAffectedApps, ref uint lpdwRebootReasons);
  [DllImport("rstrtmgr.dll")] static extern int RmEndSession(uint pSessionHandle);
  public static string[] WhoLocks(string path) {
    uint handle;
    int res = RmStartSession(out handle, 0, Guid.NewGuid().ToString());
    if (res != 0) return new string[] { "RmStartSession failed: " + res };
    try {
      res = RmRegisterResources(handle, 1, new string[] { path }, 0, IntPtr.Zero, 0, null);
      if (res != 0) return new string[] { "RmRegisterResources failed: " + res };
      uint needed = 0, count = 0, reasons = RmRebootReasonNone;
      res = RmGetList(handle, out needed, ref count, null, ref reasons);
      count = needed;
      var infos = new RM_PROCESS_INFO[count];
      res = RmGetList(handle, out needed, ref count, infos, ref reasons);
      var result = new List<string>();
      for (int i = 0; i < count; i++) result.Add(infos[i].Process.dwProcessId + " " + infos[i].strAppName + " " + infos[i].ApplicationType);
      return result.ToArray();
    } finally { RmEndSession(handle); }
  }
}
'@

    if (-not ("RestartManagerLockCheck" -as [type])) {
        Add-Type $source
    }

    [RestartManagerLockCheck]::WhoLocks((Resolve-Path -LiteralPath $Path).Path)
}

if (-not (Test-Path -LiteralPath $DocsPath)) {
    throw "Missing unstaged asset list: $DocsPath"
}

$doc = Get-Content -Raw -LiteralPath $DocsPath
$paths = [regex]::Matches($doc, '`([^`]+\.uasset)`') |
    ForEach-Object { $_.Groups[1].Value.Replace('\', '/') } |
    Sort-Object -Unique

if (-not $paths -or $paths.Count -eq 0) {
    throw "No .uasset entries found in $DocsPath"
}

New-Item -ItemType Directory -Force -Path $UploadRoot | Out-Null

$missing = @()
$copied = @()
foreach ($path in $paths) {
    if (-not (Test-Path -LiteralPath $path)) {
        $missing += $path
        continue
    }

    $dest = Join-Path $UploadRoot $path
    $destDir = Split-Path $dest -Parent
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
    Copy-Item -LiteralPath $path -Destination $dest -Force

    $srcItem = Get-Item -LiteralPath $path
    $dstItem = Get-Item -LiteralPath $dest
    if ($srcItem.Length -ne $dstItem.Length) {
        throw "Copied size mismatch: $path"
    }
    $copied += [PSCustomObject]@{
        Path = $path
        SizeMiB = [Math]::Round($srcItem.Length / 1MB, 2)
    }
}

if ($missing.Count -gt 0) {
    throw "Missing listed assets:`n$($missing -join "`n")"
}

$zipPath = Join-Path $UploadRoot $ZipName
if (Test-Path -LiteralPath $zipPath) {
    try {
        Remove-Item -LiteralPath $zipPath -Force -ErrorAction Stop
    } catch {
        $locks = Get-LockingProcesses -Path $zipPath
        throw "Could not remove existing zip '$zipPath'. Locking processes: $($locks -join '; ')"
    }
}

$contentRoot = Join-Path $UploadRoot "Content"
if (-not (Test-Path -LiteralPath $contentRoot)) {
    throw "Upload content root was not created: $contentRoot"
}

Compress-Archive -LiteralPath $contentRoot -DestinationPath $zipPath -CompressionLevel Optimal

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $zipPath).Path)
try {
    $badEntries = $zip.Entries |
        Where-Object { $_.FullName -and -not $_.FullName.StartsWith("Content\") -and -not $_.FullName.StartsWith("Content/") }
    if ($badEntries.Count -gt 0) {
        throw "Zip contains entries outside Content/: $($badEntries.FullName -join ', ')"
    }
} finally {
    $zip.Dispose()
}

$totalMiB = [Math]::Round((($copied | ForEach-Object { $_.SizeMiB } | Measure-Object -Sum).Sum), 2)
[PSCustomObject]@{
    AssetCount = $copied.Count
    SourceTotalMiB = $totalMiB
    ZipPath = $zipPath
    ZipSizeMiB = [Math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 2)
}
