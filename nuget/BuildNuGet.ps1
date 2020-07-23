param(
    [Parameter(Mandatory=$true, HelpMessage="Path to unzipped openxr_loader_windows OpenXR-SDK release asset")]
    $SDKRelease,
    $Version)

$ErrorActionPreference = "Stop"

if (-Not (Test-Path $SDKRelease)) {
    Throw "$SDKRelease not found"
}

$NugetTemplate = Join-Path $PSScriptRoot "NugetTemplate"
$NugetStaging = Join-Path $PSScriptRoot "NugetStaging"

if (Test-Path $NugetStaging) {
    Remove-Item $NugetStaging -Recurse
}

#
# Start off using the NuGet template.
#
Copy-Item $NugetTemplate $NugetStaging -Recurse

#
# Update the NuSpec
#
$NuSpecPath = Join-Path $NugetStaging "OpenXR.Loader.nuspec"
$xml = [xml](Get-Content $NuSpecPath)
$nsm = New-Object Xml.XmlNamespaceManager($xml.NameTable)
$nsm.AddNamespace("ng", "http://schemas.microsoft.com/packaging/2010/07/nuspec.xsd")
$xml.SelectSingleNode("/ng:package/ng:metadata/ng:version", $nsm).InnerText = $Version
$xml.Save($NuSpecPath)

#
# Copy in the headers from the SDK release.
#
Copy-Item (Join-Path $SDKRelease "include") (Join-Path $NugetStaging "include") -Recurse

#
# Copy in the binaries from the SDK release.
#
function CopyLoader($Platform)
{
    $PlatformSDKPath = Join-Path $SDKRelease "$Platform"
    $NuGetPlatformPath = Join-Path $NugetStaging "native/$Platform/release"

    $NugetLibPath = Join-Path $NuGetPlatformPath "lib"
    New-Item $NugetLibPath -ItemType "directory" -Force
    Copy-Item (Join-Path $PlatformSDKPath "lib/openxr_loader.lib") $NugetLibPath

    $NugetBinPath = Join-Path $NuGetPlatformPath "bin"
    New-Item $NugetBinPath -ItemType "directory" -Force
    Copy-Item (Join-Path $PlatformSDKPath "bin/openxr_loader.dll") $NugetBinPath
}

# Currently there are no non-UWP ARM/ARM64 binaries available from the SDK release.
CopyLoader "x64"
CopyLoader "Win32"
CopyLoader "x64_uwp"
CopyLoader "Win32_uwp"
CopyLoader "arm64_uwp"
CopyLoader "arm_uwp"

if (!(Get-Command "nuget.exe" -ErrorAction SilentlyContinue))
{
    Throw "NuGet.exe is missing. You can get it at https://www.nuget.org/downloads"
}

& Nuget.exe pack $NugetStaging
if (!$?)
{
    Throw "NuGet.exe failed: $LASTEXITCODE";
}