#requires -Version 5.1
<#
Synchronize prepared GodFall packs, then launch a packaged Windows game.
Run with -SyncOnly to synchronize without starting the game.
#>
[CmdletBinding()]
param(
    [string]$GameRoot,
    [string]$EditorProject,
    [string]$GameExe,
    [switch]$SyncOnly,
    [switch]$AllowLocalFallback,
    [string]$RepositoryBaseUrl = 'https://raw.githubusercontent.com/kendallryanlewis/GodFall',
    [int]$TimeoutSeconds = 45
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Owner = 'GodFallGitHubPacks-v1'
$MarkerName = '.godfall-github.json'
$MaxPackBytes = 95MB
$MaxManifestBytes = 1MB
$lockStream = $null
$client = $null
$syncFailure = $null
$resultState = 'no_change'
$resultCount = 0

function Assert-SafeDirectory([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($script:ContentRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Managed path escapes the packaged Content directory: $full"
    }
    $cursor = $full
    while ($cursor -and $cursor.StartsWith($script:RootPath, [StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $cursor) {
            $entry = Get-Item -LiteralPath $cursor -Force
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Linked paths are unsupported: $cursor" }
        }
        if ($cursor -eq $script:RootPath) { break }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
    return $full
}

function Read-Owner([string]$Directory) {
    $null = Assert-SafeDirectory $Directory
    $marker = Join-Path $Directory $script:MarkerName
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { throw "Refusing an existing directory without a GodFall ownership record: $Directory" }
    $entry = Get-Item -LiteralPath $marker -Force
    if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Ownership record must not be linked' }
    $record = Get-Content -LiteralPath $marker -Raw | ConvertFrom-Json
    if ($record.owner -cne $script:Owner) { throw "Unrecognized directory owner: $Directory" }
    return $record
}

function Write-Owner([string]$Directory, $Manifest) {
    $record = @{ owner = $script:Owner; manifest = $Manifest }
    $json = $record | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText((Join-Path $Directory $script:MarkerName), $json, (New-Object Text.UTF8Encoding($false)))
}

function Remove-Owned([string]$Directory) {
    # Resolve and verify the exact target before any recursive deletion.
    $full = Assert-SafeDirectory $Directory
    if (Test-Path -LiteralPath $full) {
        $null = Assert-ManagedShape $full
        foreach ($entry in @(Get-ChildItem -LiteralPath $full -Recurse -Force)) {
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Refusing linked managed content: $($entry.FullName)" }
        }
        Remove-Item -LiteralPath $full -Recurse -Force
    }
}

function Get-PackName($Pack) { return ($Pack.id + '_' + $Pack.sha256.Substring(0,16) + '_P.pak') }

function Assert-Manifest($Manifest) {
    if ($null -eq $Manifest -or $Manifest.schema_version -cne 1 -or $Manifest.project -cne 'GodFall_Clean' -or
        $Manifest.engine -cne '5.8' -or $Manifest.platform -cne 'Win64') { throw 'Unsupported manifest project, engine, platform, or schema' }
    if ($Manifest.source_commit -cnotmatch '^[0-9a-f]{40}$') { throw 'Manifest must pin a full source commit' }
    if ($null -eq $Manifest.packs -or $Manifest.packs -isnot [Array] -or $Manifest.packs.Count -gt 256) { throw 'Manifest packs must be an array of at most 256 entries' }
    $ids = @{}
    $paths = @{}
    $catalogs = @{}
    foreach ($pack in @($Manifest.packs)) {
        if ($pack.id -cnotmatch '^[0-9a-f]{24}$' -or $pack.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid pack ID or SHA-256' }
        if ($pack.path -cnotmatch '^GameItems/(Models|Armor|Clothing|Weapons)/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+\.pak$') { throw 'Invalid pack path' }
        if ($pack.catalog -cnotmatch '^/Game/GFExternal/(?:[A-Za-z0-9_]+/)*([A-Za-z0-9_]+)\.\1$') { throw 'Invalid external pack catalog path' }
        if (($pack.size -isnot [int] -and $pack.size -isnot [long]) -or $pack.size -le 0 -or $pack.size -gt $script:MaxPackBytes) { throw 'Invalid or oversized pack' }
        if ($ids.ContainsKey($pack.id) -or $paths.ContainsKey($pack.path) -or $catalogs.ContainsKey($pack.catalog)) { throw 'Duplicate pack identity, path or catalog' }
        $ids[$pack.id] = $true
        $paths[$pack.path] = $true
        $catalogs[$pack.catalog] = $true
    }
}

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}

function Test-Pack([string]$Path, $Pack) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    $entry = Get-Item -LiteralPath $Path -Force
    if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $entry.Length -ne $Pack.size) { return $false }
    return ((Get-Sha256 $Path) -ceq $Pack.sha256)
}

function Assert-ManagedShape([string]$Directory) {
    $record = Read-Owner $Directory
    $expected = @($script:MarkerName)
    if ($null -ne $record.manifest) {
        Assert-Manifest $record.manifest
        foreach ($pack in @($record.manifest.packs)) { $expected += (Get-PackName $pack) }
    }
    foreach ($entry in @(Get-ChildItem -LiteralPath $Directory -Force)) {
        if ($entry.PSIsContainer -or $entry.Name -notin $expected -or
            ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Managed directory contains unrecognized or linked content' }
    }
    return $record
}

function Assert-Cache([string]$Directory) {
    $record = Assert-ManagedShape $Directory
    Assert-Manifest $record.manifest
    foreach ($pack in @($record.manifest.packs)) {
        if (-not (Test-Pack (Join-Path $Directory (Get-PackName $pack)) $pack)) { throw 'Last successful pack set is incomplete or corrupt' }
    }
    return $record.manifest
}

function Save-Download([string]$Url, [string]$Destination, [long]$Maximum) {
    $response = $script:client.GetAsync($Url, [Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
    try {
        $null = $response.EnsureSuccessStatusCode()
        if ($response.Content.Headers.ContentLength -gt $Maximum) { throw 'Download exceeds its allowed size' }
        $inputStream = $response.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        try {
            $outputStream = [IO.File]::Open($Destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            try {
                $buffer = New-Object byte[] 65536
                [long]$total = 0
                $deadline = [DateTime]::UtcNow.AddSeconds($script:TimeoutSeconds)
                while ($true) {
                    $remaining = [int]($deadline - [DateTime]::UtcNow).TotalMilliseconds
                    if ($remaining -le 0) { throw 'Download timed out' }
                    $task = $inputStream.ReadAsync($buffer, 0, $buffer.Length)
                    if (-not $task.Wait($remaining)) { throw 'Download timed out' }
                    $count = $task.GetAwaiter().GetResult()
                    if ($count -eq 0) { break }
                    $total += $count
                    if ($total -gt $Maximum) { throw 'Download exceeds its allowed size' }
                    $outputStream.Write($buffer, 0, $count)
                }
            } finally { $outputStream.Dispose() }
        } finally { $inputStream.Dispose() }
    } finally { $response.Dispose() }
}

try {
    if ($EditorProject) {
        if ($GameRoot -or $GameExe -or -not $SyncOnly) { throw 'Editor cache synchronization requires only -EditorProject and -SyncOnly' }
        $projectFile = [IO.Path]::GetFullPath($EditorProject)
        if ([IO.Path]::GetFileName($projectFile) -cne 'GodFall_Clean.uproject' -or -not (Test-Path -LiteralPath $projectFile -PathType Leaf)) { throw 'EditorProject must name the existing GodFall_Clean.uproject' }
        $RootPath = [IO.Path]::GetDirectoryName($projectFile)
        $ContentRoot = [IO.Path]::GetFullPath((Join-Path $RootPath 'Saved/GodFallGitHubEditor'))
    } else {
    if (-not $GameRoot) { throw 'Provide a packaged GameRoot or an explicit EditorProject' }
    $RootPath = [IO.Path]::GetFullPath($GameRoot).TrimEnd([char[]]'\/')
    # Require a packaged project directory, not a live Unreal source project.
    if (-not (Test-Path -LiteralPath (Join-Path $RootPath 'Content/Paks') -PathType Container)) { throw 'GameRoot must contain the packaged GodFall_Clean Content/Paks directory' }
    if (@(Get-ChildItem -LiteralPath $RootPath -Filter '*.uproject' -File).Count -gt 0) { throw 'Use a packaged game directory, not the live Unreal project' }
    $ContentRoot = [IO.Path]::GetFullPath((Join-Path $RootPath 'Content'))
    }
    $Active = Join-Path $ContentRoot 'Paks/GodFallGitHub'
    $Work = Join-Path $ContentRoot 'GodFallGitHubSync'
    $Pending = Join-Path $Work 'pending'
    $Previous = Join-Path $Work 'previous'
    if ([Math]::Max($Active.Length, $Pending.Length) + 51 -gt 245) { throw 'Use a shorter packaged game path for Windows PowerShell 5.1' }
    $null = Assert-SafeDirectory $Active
    $null = Assert-SafeDirectory $Work
    if ($EditorProject) {
        $pakParent = Assert-SafeDirectory (Join-Path $ContentRoot 'Paks')
        if (-not (Test-Path -LiteralPath $pakParent)) { $null = New-Item -ItemType Directory -Path $pakParent -Force }
    }
    if (Test-Path -LiteralPath $Work) { $null = Read-Owner $Work }
    else { $null = New-Item -ItemType Directory -Path $Work; Write-Owner $Work $null }
    $lockStream = [IO.File]::Open((Join-Path $Work 'sync.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    # Keep cached definitions for existing saves, but new loot/store acquisition falls
    # back to local items unless this startup receives and validates a remote manifest.
    $unavailable = Join-Path $Work 'acquisition-unavailable'
    if ((Test-Path -LiteralPath $unavailable) -and ((Get-Item -LiteralPath $unavailable -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Availability marker must not be linked' }
    [IO.File]::WriteAllText($unavailable, 'GitHub synchronization has not succeeded for this launch.')
    foreach ($process in @(Get-Process -Name 'GodFall_Clean*' -ErrorAction SilentlyContinue)) {
        if ($process.Path -and $process.Path.StartsWith($RootPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Close this GodFall game before synchronizing its packs' }
    }
    if (-not $SyncOnly) {
        if (-not $GameExe) { throw 'Provide -GameExe for a packaged GodFall executable, or use -SyncOnly' }
        $ExePath = (Resolve-Path -LiteralPath $GameExe).Path
        if ([IO.Path]::GetFileName($ExePath) -cnotmatch '^GodFall_Clean(?:-Win64-(?:Shipping|Development))?\.exe$') { throw 'GameExe must be a GodFall_Clean Windows executable' }
        $parent = [IO.Path]::GetDirectoryName($RootPath)
        if (-not $ExePath.StartsWith($parent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'GameExe must belong to this packaged build' }
    }
    if (Test-Path -LiteralPath $Active) { $null = Assert-ManagedShape $Active }
    if (Test-Path -LiteralPath $Previous) {
        $null = Assert-Cache $Previous
        if (-not (Test-Path -LiteralPath $Active)) { Move-Item -LiteralPath $Previous -Destination $Active }
        else { $null = Assert-Cache $Active; Remove-Owned $Previous }
    }
    $null = Remove-Owned $Pending
    $base = [Uri]$RepositoryBaseUrl
    if ($base.Scheme -ne 'https' -and -not ($base.Scheme -eq 'http' -and $base.IsLoopback)) { throw 'Repository URL must use HTTPS' }
    if ($base.UserInfo -or $base.Query -or $base.Fragment) { throw 'Repository URL must not contain credentials, a query, or a fragment' }
    Add-Type -AssemblyName System.Net.Http
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $client = New-Object Net.Http.HttpClient
    $client.Timeout = [TimeSpan]::FromSeconds($TimeoutSeconds)
    $client.DefaultRequestHeaders.UserAgent.ParseAdd('GodFallContentSync/1.0')
    $null = New-Item -ItemType Directory -Path $Pending
    Write-Owner $Pending $null
    try {
        $manifestFile = Join-Path $Work 'manifest.download'
        if (Test-Path -LiteralPath $manifestFile) { Remove-Item -LiteralPath $manifestFile -Force }
        Save-Download ($RepositoryBaseUrl.TrimEnd('/') + '/main/GameItems/manifest.json') $manifestFile $MaxManifestBytes
        $manifest = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
        Assert-Manifest $manifest
        Write-Owner $Pending $manifest
        $unchanged = $false
        if (Test-Path -LiteralPath $Active) {
            try {
                $cached = Assert-Cache $Active
                $unchanged = (($cached | ConvertTo-Json -Depth 12 -Compress) -ceq ($manifest | ConvertTo-Json -Depth 12 -Compress))
            } catch { $unchanged = $false }
        }
        if (-not $unchanged) {
            foreach ($pack in @($manifest.packs)) {
                $name = Get-PackName $pack
                $destination = Join-Path $Pending $name
                $existing = Join-Path $Active $name
                if (Test-Pack $existing $pack) { Copy-Item -LiteralPath $existing -Destination $destination }
                else {
                    $url = $RepositoryBaseUrl.TrimEnd('/') + '/' + $manifest.source_commit + '/' + $pack.path
                    Save-Download $url $destination $pack.size
                }
                if (-not (Test-Pack $destination $pack)) { throw "Pack checksum or size mismatch: $($pack.path)" }
            }
            $null = Assert-Cache $Pending
            # Both renames stay on the packaged game's volume. Retain rollback until promotion succeeds.
            if (Test-Path -LiteralPath $Active) { Move-Item -LiteralPath $Active -Destination $Previous }
            try { Move-Item -LiteralPath $Pending -Destination $Active }
            catch {
                if ((Test-Path -LiteralPath $Previous) -and -not (Test-Path -LiteralPath $Active)) { Move-Item -LiteralPath $Previous -Destination $Active }
                throw
            }
            $resultState = 'updated'
        }
        $resultCount = @($manifest.packs).Count
    } catch {
        $syncFailure = $_.Exception.Message
        if (-not (Test-Path -LiteralPath $Active) -and $AllowLocalFallback) {
            $resultCount = 0
            $resultState = 'local'
            Write-Warning "Optional items unavailable; using local armor and weapons. $syncFailure"
        } else {
        if (-not (Test-Path -LiteralPath $Active)) { throw }
        $cached = Assert-Cache $Active
        $resultCount = @($cached.packs).Count
        $resultState = 'cached'
        Write-Warning "Sync failed; retaining the last validated pack set. $syncFailure"
        }
    }
    Remove-Owned $Pending
    Remove-Owned $Previous
    if (-not $syncFailure) { Remove-Item -LiteralPath $unavailable -Force }
    Write-Output "GODFALL_SYNC state=$resultState packs=$resultCount"
    if (-not $SyncOnly) {
        # The requested game is interactive and must open normally for the player.
        $null = Start-Process -FilePath $ExePath -WorkingDirectory ([IO.Path]::GetDirectoryName($ExePath))
        Write-Output 'GodFall launched after content validation.'
    }
} catch {
    Write-Error ('GODFALL_SYNC_FAILED: ' + $_.Exception.Message) -ErrorAction Continue
    exit 1
} finally {
    if ($null -ne $client) { $client.Dispose() }
    if ($null -ne $lockStream) { $lockStream.Dispose() }
}
