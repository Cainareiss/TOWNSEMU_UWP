param(
    [string]$Version = '0.1.0.0',
    [string]$Exe = 'E:\claude-work\townsuwp\repo\build\Release\tsugaru_uwp.exe',
    [string]$OutDir = 'E:\claude-work\townsuwp\out',
    [string]$CertThumbprint = '273F3FCE00EAC11F0AA54F739D2ACCE5839DCC33'   # CN=irixaligned, CurrentUser\My (same signer as the other Xbox ports)
)

$ErrorActionPreference = 'Stop'
$repo = 'E:\claude-work\townsuwp\repo'
$pkg = Join-Path $repo 'uwp\package'
$sdk = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
$stage = 'E:\claude-work\townsuwp\stage'
$env:TEMP = 'E:\claude-work\tmp'; $env:TMP = $env:TEMP

foreach ($p in @($Exe, "$sdk\makeappx.exe", "$sdk\signtool.exe", (Join-Path $pkg 'AppxManifest.xml'), (Join-Path $pkg 'Assets\StoreLogo.png'))) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing: $p" }
}

# 1. Stage: exe, manifest, logos, the frontend's assets.
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item $Exe $stage
Copy-Item (Join-Path $pkg 'AppxManifest.xml') $stage
Copy-Item (Join-Path $pkg 'Assets') (Join-Path $stage 'Assets') -Recurse
# Frontend assets (fonts, icons, 3D case, sounds, index files). The LibCrypt .sbi files are PS1-only: left out.
robocopy (Join-Path $repo 'uwp\swanui\assets') (Join-Path $stage 'swanui\assets') /E /XD sbi_files /R:0 /W:0 /NJH /NJS /NP /NFL /NDL | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy assets failed: $LASTEXITCODE" }
# The system ROMs, built into the package (the user's own set from E:\Downloads\fm; personal package, never share it).
# The host (host_tsugaru.cpp find_rom_dir) reads <install>\swanui\rom when the data root has no bios folder with ROMs.
$romSrc = 'E:\claude-work\townsuwp\userbios'
foreach ($rom in 'FMT_SYS.ROM', 'FMT_DOS.ROM', 'FMT_FNT.ROM', 'FMT_DIC.ROM', 'FMT_F20.ROM') {
    if (-not (Test-Path (Join-Path $romSrc $rom))) { throw "ROM missing: $romSrc\$rom" }
}
New-Item -ItemType Directory -Path (Join-Path $stage 'swanui\rom') -Force | Out-Null
Copy-Item (Join-Path $romSrc '*.ROM') (Join-Path $stage 'swanui\rom')
# Any DLL next to the exe (none expected): must be flagged AppContainer.
Get-ChildItem (Split-Path $Exe) -Filter *.dll -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName $stage }

# 2. Version into the manifest.
$manifestPath = Join-Path $stage 'AppxManifest.xml'
$m = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8
$m = [regex]::Replace($m, '(<Identity[^>]*Version=")[0-9.]+(")', "`${1}$Version`${2}", 1)
[IO.File]::WriteAllText($manifestPath, $m, (New-Object Text.UTF8Encoding $true))

# 3. Check the exe: AppContainer flag and imports (an earlier port failed to load because of desktop DLLs).
$dumpbin = (Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe' | Sort-Object FullName -Descending | Select-Object -First 1).FullName
$hdr = & $dumpbin /NOLOGO /HEADERS (Join-Path $stage 'tsugaru_uwp.exe')
if (-not ($hdr -match 'App Container')) { throw 'tsugaru_uwp.exe lacks the AppContainer flag.' }
$deps = (& $dumpbin /NOLOGO /DEPENDENTS (Join-Path $stage 'tsugaru_uwp.exe')) | Where-Object { $_ -match '\.dll' } | ForEach-Object { $_.Trim() }
Write-Host ("Imports: " + ($deps -join ', '))
$bad = $deps | Where-Object { $_ -match '^(MSVCP140|VCRUNTIME140|ucrtbase|d3dcompiler|user32|gdi32|winmm|dsound|opengl32|glu32|ole32|shell32)(_1|_2)?\.dll$' }
if ($bad) { throw "Desktop-only imports: $($bad -join ', ')" }

# 4. Pack and sign.
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$msix = Join-Path $OutDir "Tsugaru-FMTowns-Xbox-x64-$Version.msix"
if (Test-Path $msix) { Remove-Item $msix -Force }
& "$sdk\makeappx.exe" pack /o /d $stage /p $msix
if ($LASTEXITCODE -ne 0) { throw "makeappx failed: $LASTEXITCODE" }
& "$sdk\signtool.exe" sign /fd SHA256 /sha1 $CertThumbprint $msix
if ($LASTEXITCODE -ne 0) { throw "signtool failed: $LASTEXITCODE" }

# 5. Companion files: public certificate and the VCLibs dependency.
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object Thumbprint -eq $CertThumbprint
Export-Certificate -Cert $cert -FilePath (Join-Path $OutDir 'Tsugaru-Dev.cer') -Force | Out-Null
$vclibs = '<your packages folder>\Dependencies\Microsoft.VCLibs.x64.14.00.appx'
if (Test-Path $vclibs) {
    New-Item -ItemType Directory -Path (Join-Path $OutDir 'Dependencies') -Force | Out-Null
    Copy-Item $vclibs (Join-Path $OutDir 'Dependencies') -Force
}

# 6. Structural check.
$check = 'E:\claude-work\townsuwp\check'
if (Test-Path $check) { Remove-Item $check -Recurse -Force }
& "$sdk\makeappx.exe" unpack /o /p $msix /d $check | Out-Null
if ($LASTEXITCODE -ne 0) { throw "makeappx unpack failed: $LASTEXITCODE" }
foreach ($must in @('AppxManifest.xml', 'tsugaru_uwp.exe', 'Assets\Square150x150Logo.png', 'Assets\SplashScreen.png', 'swanui\assets\flamenco-emblem.png', 'swanui\rom\FMT_SYS.ROM', 'swanui\rom\FMT_DOS.ROM')) {
    if (-not (Test-Path (Join-Path $check $must))) { throw "Missing in package: $must" }
}
$sig = Get-AuthenticodeSignature $msix
Write-Host ("MSIX: {0}  ({1:N1} MB)  signer={2}  status={3}" -f $msix, ((Get-Item $msix).Length / 1MB), $sig.SignerCertificate.Subject, $sig.Status)
Write-Host "Certificate: $(Join-Path $OutDir 'Tsugaru-Dev.cer')"
