$pattern = "eROIL_Tests"   # or "eROIL" to match substring

$procs = Get-Process -ErrorAction SilentlyContinue |
  Where-Object { $_.ProcessName -like "*$pattern*" }

if (-not $procs) {
  Write-Host "No running processes matched '*$pattern*'." -ForegroundColor Yellow
  exit 0
}

# build rows with numeric properties
$rows = $procs | Select-Object `
  Id, ProcessName,
  @{n="WorkingSetBytes"; e={ [int64]$_.WorkingSet64 } },
  @{n="Threads";        e={ [int]$_.Threads.Count } }

# show per-process
$rows | Select-Object Id, ProcessName,
  @{n="WorkingSetMB"; e={[math]::Round($_.WorkingSetBytes/1MB, 2)}},
  Threads |
  Sort-Object WorkingSetMB -Descending

# correct totals (measure each property separately)
$totalWS      = ($rows | Measure-Object -Property WorkingSetBytes -Sum).Sum
$totalThreads = ($rows | Measure-Object -Property Threads        -Sum).Sum

"`nTOTAL:"
[pscustomobject]@{
  Instances    = $rows.Count
  WorkingSetMB = [math]::Round($totalWS / 1MB, 2)
  Threads      = $totalThreads
}
