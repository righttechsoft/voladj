param([switch]$Uninstall)

$id = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $id.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "voladj install.ps1 must be run from an elevated (Administrator) PowerShell."
    exit 1
}

if ($Uninstall) {
    Stop-Process -Name voladj -Force -ErrorAction SilentlyContinue
    Unregister-ScheduledTask -TaskName voladj -Confirm:$false -ErrorAction SilentlyContinue
    exit 0
}

$exe = Join-Path $PSScriptRoot 'voladj.exe'
if (-not (Test-Path $exe)) {
    Write-Error "voladj.exe not found next to the script. Run build.bat first."
    exit 1
}
$user = "$env:USERDOMAIN\$env:USERNAME"
$action = New-ScheduledTaskAction -Execute $exe
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $user
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit 0 -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
Register-ScheduledTask -TaskName voladj -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName voladj
