param([Parameter(Mandatory)][string]$Path)
$ErrorActionPreference='Stop'
$document=Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
if ($document.schema -ne 1 -or $document.vid -ne '06A3' -or $document.pid -ne '075C') { throw 'Expected a schema 1 PS28 diagnostic capture.' }
$reports=@($document.capture | Where-Object event -eq report)
$ranges=if($reports.Count) {
    foreach($property in $reports[0].decoded.PSObject.Properties) {
        $id=$property.Name
        $range=$reports | ForEach-Object { $_.decoded.$id.raw } | Measure-Object -Minimum -Maximum
        [pscustomobject]@{HidControl=$id;Minimum=$range.Minimum;Maximum=$range.Maximum}
    }
}
[pscustomobject]@{
    Path=(Resolve-Path -LiteralPath $Path).Path
    Reports=$reports.Count
    FirstUtc=if($reports.Count){$reports[0].utc}else{$null}
    LastUtc=if($reports.Count){$reports[-1].utc}else{$null}
    InvalidReports=@($reports | Where-Object { -not $_.valid }).Count
    Events=@($document.capture | Where-Object event -ne report | Select-Object event,utc)
    Ranges=@($ranges)
    Analysis=$document.analysis
    Note='Raw spans and manual markers are observations; they do not establish a fault signature.'
} | ConvertTo-Json -Depth 20
