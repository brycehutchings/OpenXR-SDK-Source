param(
    [Parameter(Mandatory=$true, HelpMessage="Path to unzipped openxr_loader_windows OpenXR-SDK release asset")]
    $SDKRelease,
    [Parameter(Mandatory=$true, HelpMessage="Path to specification Makefile. Needed to extract the version")]
    $SpecMakefile,
    [Parameter(Mandatory=$true, HelpMessage="Path create staged nuget directory layout")]
    $NugetStaging,
    [Parameter(HelpMessage="Version revision number")]
    $VersionRevision = "0")

$ErrorActionPreference = "Stop"

if (-Not (Test-Path $SDKRelease)) {
    Throw "SDK Release folder not found: $SDKRelease"
}
if (-Not (Test-Path $SpecMakefile)) {
    Throw "Specification makefile not found: $SpecMakefile"
}

$NugetTemplate = Join-Path $PSScriptRoot "NugetHeadersTemplate"

if (Test-Path $NugetStaging) {
    Remove-Item $NugetStaging -Recurse
}

#
# Extract version from Specification makefile
#
$VersionMatch = Select-String -Path $SpecMakefile -Pattern "^SPECREVISION\s*=\s*(.+)"
$SDKVersion = $VersionMatch.Matches[0].Groups[1]

# Append on the revision and optional suffix
$SDKVersion = "$SDKVersion.$VersionRevision"

#
# Start off using the NuGet template.
#
echo "Copy-Item $NugetTemplate $NugetStaging -Recurse"
Copy-Item $NugetTemplate $NugetStaging -Recurse

#
# Update the NuSpec
#
$NuSpecPath = Resolve-Path (Join-Path $NugetStaging "OpenXR.Headers.nuspec")
$xml = [xml](Get-Content $NuSpecPath)
$nsm = New-Object Xml.XmlNamespaceManager($xml.NameTable)
$nsm.AddNamespace("ng", "http://schemas.microsoft.com/packaging/2010/07/nuspec.xsd")
$xml.SelectSingleNode("/ng:package/ng:metadata/ng:version", $nsm).InnerText = $SDKVersion
$xml.Save($NuSpecPath)

#
# Copy in the headers from the SDK release.
#
Copy-Item (Join-Path $SDKRelease "include") (Join-Path $NugetStaging "include") -Recurse