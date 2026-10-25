#requires -Version 5.1
<# Download a complete immutable GodFall build from GitHub, then synchronize optional packs. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$InstallRoot,
    [switch]$SyncOnly,
    [string]$RepositoryBaseUrl='https://raw.githubusercontent.com/kendallryanlewis/GodFall',
    [string]$ReleaseBaseUrl='https://github.com/kendallryanlewis/GodFall/releases/download',
    [string]$ItemRepositoryBaseUrl='https://raw.githubusercontent.com/kendallryanlewis/GodFall',
    [string[]]$GameArguments=@()
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$Owner='GodFallFullContent-v1'
$lock=$null

function Assert-Directory([string]$Path) {
    $full=[IO.Path]::GetFullPath($Path)
    if ($full -ne $script:Root -and -not $full.StartsWith($script:Root+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Path escapes the owned install'}
    $cursor=$full
    while($cursor) {
        if(Test-Path -LiteralPath $cursor) {
            $item=Get-Item -LiteralPath $cursor -Force
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Linked install path refused: $cursor"}
            if($item.PSIsContainer -and @(Get-ChildItem -LiteralPath $cursor -Filter '*.uproject' -File).Count){throw 'Live Unreal project paths are not installation targets'}
        }
        $parent=[IO.Path]::GetDirectoryName($cursor)
        if($parent -eq $cursor){break};$cursor=$parent
    }
    return $full
}
function Write-Json([string]$Path,$Value) {
    $temporary=$Path+'.new'
    [IO.File]::WriteAllText($temporary,($Value|ConvertTo-Json -Depth 16),(New-Object Text.UTF8Encoding($false)))
    if(Test-Path -LiteralPath $Path){[IO.File]::Replace($temporary,$Path,$Path+'.previous')}else{[IO.File]::Move($temporary,$Path)}
}
function Hash-File([string]$Path){
    $stream=[IO.File]::OpenRead($Path);$sha=[Security.Cryptography.SHA256]::Create()
    try{return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose();$stream.Dispose()}
}
function Assert-UrlBase([string]$Value) {
    $uri=New-Object Uri($Value)
    if($uri.Scheme -ne 'https' -and -not($uri.Scheme -eq 'http' -and $uri.IsLoopback)){throw 'Use HTTPS, or explicit loopback test URLs'}
    if($uri.UserInfo -or $uri.Query -or $uri.Fragment){throw 'Invalid download base URL'}
}
function Fetch([string]$Url,[string]$Destination,[long]$Limit) {
    $request=[Net.HttpWebRequest]::Create($Url);$request.Timeout=600000;$request.ReadWriteTimeout=120000
    $response=$null;$inputStream=$null;$outputStream=$null
    try {
        $response=$request.GetResponse()
        if($response.ContentLength -gt $Limit){throw 'Download exceeds declared size limit'}
        $inputStream=$response.GetResponseStream();$outputStream=[IO.File]::Create($Destination)
        $buffer=New-Object byte[] (4MB);[long]$total=0
        while(($count=$inputStream.Read($buffer,0,$buffer.Length)) -gt 0){$total+=$count;if($total -gt $Limit){throw 'Download exceeds size limit'};$outputStream.Write($buffer,0,$count)}
    } finally {if($outputStream){$outputStream.Dispose()};if($inputStream){$inputStream.Dispose()};if($response){$response.Dispose()}}
}
function Validate-Manifest($Manifest) {
    if($Manifest.schema_version -ne 1 -or $Manifest.project -cne 'GodFall_Clean' -or $Manifest.platform -cne 'Win64' -or $Manifest.engine -cne '5.8'){throw 'Incompatible full-content manifest'}
    if($Manifest.release_tag -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.-]{0,79}$' -or $Manifest.source_commit -cnotmatch '^[a-f0-9]{40}$' -or $Manifest.executable -cne 'GodFall_Clean.exe'){throw 'Invalid release identity'}
    if($Manifest.archive.sha256 -cnotmatch '^[a-f0-9]{64}$' -or $Manifest.archive.size -le 0 -or $Manifest.archive.size -gt 50GB){throw 'Invalid archive identity'}
    $parts=@($Manifest.archive.parts);$files=@($Manifest.files)
    if($parts.Count -lt 1 -or $parts.Count -gt 100 -or $files.Count -lt 2 -or $files.Count -gt 50000){throw 'Invalid release entry count'}
    $names=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    [long]$total=0
    foreach($part in $parts){if($part.name -cnotmatch '^GodFall-Windows\.zip\.part[0-9]{3}$' -or -not $names.Add($part.name) -or $part.size -le 0 -or $part.size -ge 2GB -or $part.sha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Invalid release part'};$total+=$part.size}
    if($total -ne $Manifest.archive.size){throw 'Archive parts do not match total size'}
    $names.Clear();[long]$expanded=0
    foreach($file in $files){
        if($file.path -match '(^/|\\|:|[\x00-\x1f]|(^|/)\.{1,2}(/|$))' -or $file.path.EndsWith('/') -or -not $names.Add($file.path) -or $file.size -lt 0 -or $file.sha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Unsafe/duplicate release file'}
        foreach($segment in $file.path.Split('/')){if(-not $segment -or $segment.EndsWith('.') -or $segment.EndsWith(' ') -or $segment -match '^(?i:con|prn|aux|nul|com[0-9]|lpt[0-9])(?:\.|$)'){throw 'Invalid Windows path'}}
        if($file.path -match '(?i)(^|/)Saved/|\.sav$|\.uproject$|(^|/)GodFallGitHub(Sync)?/'){throw 'User data or source project in release'}
        $expanded+=$file.size
    }
    if($expanded -gt 50GB -or -not $names.Contains($Manifest.executable) -or -not @($files|Where-Object {$_.path -cmatch '^GodFall_Clean/Content/Paks/.+\.pak$'}).Count){throw 'Incomplete game release'}
}
function Test-Version([string]$Directory,$Manifest) {
    try {
        $null=Assert-Directory $Directory
        $marker=Join-Path $Directory '.godfall-release-version.json'
        $record=Get-Content -LiteralPath $marker -Raw|ConvertFrom-Json
        if($record.owner -cne $script:Owner -or $record.manifest.archive.sha256 -cne $Manifest.archive.sha256){return $false}
        foreach($file in $Manifest.files){
            $path=Join-Path $Directory $file.path
            $null=Assert-Directory $path
            if(-not(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -ne $file.size -or (Hash-File $path) -cne $file.sha256){return $false}
        }
        return $true
    } catch {return $false}
}
function Sync-Items([string]$VersionDirectory) {
    $itemLauncher=Join-Path $PSScriptRoot 'Start-GodFall.ps1'
    if(-not(Test-Path -LiteralPath $itemLauncher)){throw 'Companion Start-GodFall.ps1 is missing'}
    & powershell.exe -NoProfile -ExecutionPolicy RemoteSigned -File $itemLauncher -GameRoot (Join-Path $VersionDirectory 'GodFall_Clean') -SyncOnly -RepositoryBaseUrl $ItemRepositoryBaseUrl
    if($LASTEXITCODE -ne 0){throw 'Additional content synchronization failed; game was not launched'}
}

try {
    foreach($url in @($RepositoryBaseUrl,$ReleaseBaseUrl,$ItemRepositoryBaseUrl)){Assert-UrlBase $url}
    $Root=[IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
    if($Root -eq [IO.Path]::GetPathRoot($Root).TrimEnd('\')){throw 'A dedicated install folder is required'}
    $null=Assert-Directory $Root
    $ownerPath=Join-Path $Root '.godfall-release-install.json'
    if(Test-Path -LiteralPath $Root){
        if(-not(Test-Path -LiteralPath $ownerPath)){throw 'Existing install folder has no GodFall ownership marker'}
        if((Get-Content -LiteralPath $ownerPath -Raw|ConvertFrom-Json).owner -cne $Owner){throw 'Install ownership mismatch'}
    }else{New-Item -ItemType Directory -Path $Root|Out-Null;Write-Json $ownerPath @{owner=$Owner}}
    $lock=[IO.File]::Open((Join-Path $Root '.sync.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $activeProcesses=@(Get-Process -Name 'GodFall_Clean','GodFall_Clean-Win64-Shipping' -ErrorAction SilentlyContinue)
    if($activeProcesses.Count){throw 'Close GodFall before updating'}
    $versions=Join-Path $Root 'versions';$downloads=Join-Path $Root 'downloads';$userDir=Join-Path $Root 'PlayerData'
    foreach($directory in @($versions,$downloads,$userDir)){$null=Assert-Directory $directory;New-Item -ItemType Directory -Path $directory -Force|Out-Null}
    $currentPath=Join-Path $Root 'current.json';$previous=$null
    if(Test-Path -LiteralPath $currentPath){$previous=Get-Content -LiteralPath $currentPath -Raw|ConvertFrom-Json;if($previous.owner -cne $Owner){throw 'Current pointer ownership mismatch'};Validate-Manifest $previous.manifest}
    $state='unchanged';$manifest=$null;$versionPath=$null
    try {
        $manifestTemp=Join-Path $downloads 'manifest.pending.json'
        Fetch ($RepositoryBaseUrl.TrimEnd('/')+'/main/GameContent/manifest.json') $manifestTemp 16MB
        $manifest=Get-Content -LiteralPath $manifestTemp -Raw|ConvertFrom-Json;Validate-Manifest $manifest
        $id=$manifest.archive.sha256.Substring(0,16);$versionPath=Join-Path $versions $id
        if(-not(Test-Version $versionPath $manifest)){
            if(Test-Path -LiteralPath $versionPath){throw 'Existing content version is corrupt; refusing to overwrite it'}
            $cache=Join-Path $downloads $id;$null=Assert-Directory $cache;New-Item -ItemType Directory -Path $cache -Force|Out-Null
            foreach($part in $manifest.archive.parts){
                $partPath=Join-Path $cache $part.name
                if(-not(Test-Path -LiteralPath $partPath) -or (Get-Item -LiteralPath $partPath).Length -ne $part.size -or (Hash-File $partPath) -cne $part.sha256){
                    Write-Host ('Downloading '+$part.name+' ...')
                    $pending=$partPath+'.pending'
                    Fetch ($ReleaseBaseUrl.TrimEnd('/')+'/'+$manifest.release_tag+'/'+$part.name) $pending $part.size
                    if((Get-Item -LiteralPath $pending).Length -ne $part.size -or (Hash-File $pending) -cne $part.sha256){throw 'Release part checksum mismatch'}
                    if(Test-Path -LiteralPath $partPath){[IO.File]::Replace($pending,$partPath,$partPath+'.previous')}else{[IO.File]::Move($pending,$partPath)}
                }
            }
            $zip=Join-Path $cache 'GodFall-Windows.zip';$zipStream=[IO.File]::Create($zip)
            try{foreach($part in $manifest.archive.parts){$inputStream=[IO.File]::OpenRead((Join-Path $cache $part.name));try{$inputStream.CopyTo($zipStream)}finally{$inputStream.Dispose()}}}finally{$zipStream.Dispose()}
            if((Get-Item -LiteralPath $zip).Length -ne $manifest.archive.size -or (Hash-File $zip) -cne $manifest.archive.sha256){throw 'Combined release checksum mismatch'}
            $stage=Join-Path $versions ('.staging-'+[Guid]::NewGuid().ToString('N'));$null=Assert-Directory $stage
            New-Item -ItemType Directory -Path $stage|Out-Null
            $expected=@{};foreach($file in $manifest.files){$expected[$file.path]=$file}
            $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
            $archive=[IO.Compression.ZipFile]::OpenRead($zip)
            try {
                foreach($entry in $archive.Entries){
                    if(-not $expected.ContainsKey($entry.FullName) -or -not $seen.Add($entry.FullName) -or $entry.Length -ne $expected[$entry.FullName].size -or (($entry.ExternalAttributes -shr 16) -band 61440) -eq 40960){throw 'Archive entries differ from declared safe file list'}
                    $target=[IO.Path]::GetFullPath((Join-Path $stage $entry.FullName))
                    if(-not $target.StartsWith($stage+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escapes staging'}
                    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force|Out-Null
                    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$false)
                    if((Hash-File $target) -cne $expected[$entry.FullName].sha256){throw 'Extracted file checksum mismatch'}
                }
                if($seen.Count -ne $expected.Count){throw 'Archive is missing declared game files'}
            } finally {$archive.Dispose()}
            Write-Json (Join-Path $stage '.godfall-release-version.json') @{owner=$Owner;manifest=$manifest}
            [IO.Directory]::Move($stage,$versionPath)
            if(-not(Test-Version $versionPath $manifest)){throw 'Promoted release failed verification'}
            $state='updated'
        }
        # The active pointer changes only after both the complete release and its
        # optional item set are ready. A failed pack update can retain the old build.
        Sync-Items $versionPath
        Write-Json $currentPath @{owner=$Owner;manifest=$manifest}
    } catch {
        $reason=$_.Exception.Message
        if($previous){
            $candidate=Join-Path $versions $previous.manifest.archive.sha256.Substring(0,16)
            if(Test-Version $candidate $previous.manifest){$manifest=$previous.manifest;$versionPath=$candidate;Sync-Items $versionPath;$state='cached';Write-Warning ('Update unavailable; using verified installed release. '+$reason)}else{throw ('No verified release available. '+$reason)}
        }else{throw ('First installation could not complete. '+$reason)}
    }
    Write-Host ('GODFALL_RELEASE state='+$state+' version='+$manifest.archive.sha256+' files='+@($manifest.files).Count)
    if(-not $SyncOnly){
        $argumentText='-UserDir="'+$userDir+'"'
        foreach($argument in $GameArguments){if($argument -match '(?i)^-UserDir'){throw 'Player data directory is managed by the launcher'};$argumentText+=' '+$argument}
        Start-Process -FilePath (Join-Path $versionPath $manifest.executable) -WorkingDirectory $versionPath -ArgumentList $argumentText|Out-Null
    }
} finally {if($lock){$lock.Dispose()}}
