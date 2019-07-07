$start = (Get-Date).AddSeconds(-2)

"Removing old policy..."
$r = Invoke-CimMethod -Namespace root/microsoft/Windows/CI -ClassName PS_UpdateAndCompareCIPolicy -MethodName Delete

"Merging new policy..."
$m = Merge-CIPolicy -PolicyPaths `
    C:\Windows\schemas\CodeIntegrity\ExamplePolicies\AllowMicrosoft.xml, `
    .\policy.xml `
    -OutputFilePath merged.xml
if (-not $?) {
    exit
}

"Updating options..."

# Set audit mode
#Set-RuleOption .\merged.xml -Option 3
Set-RuleOption .\merged.xml -Option 3 -Delete

# Do not disable scripts or everything will be blocked
Set-RuleOption .\merged.xml -Option 11 -Delete

# Disable inherited policy
Set-RuleOption .\merged.xml -Option 5 -Delete

# Enable rebootless updates
Set-RuleOption .\merged.xml -Option 16

Set-CIPolicyIdInfo .\merged.xml -PolicyName "SPython Code Integrity Demo" -PolicyId (Get-Date)

# Updating policy
"Updating current policy..."
$p = ConvertFrom-CIPolicy merged.xml C:\Windows\System32\CodeIntegrity\SiPolicy.bin
$r = Invoke-CimMethod -Namespace root/microsoft/Windows/CI -ClassName PS_UpdateAndCompareCIPolicy -MethodName Update -Arguments @{ FilePath = $p }

if ($r.ReturnValue) {
    "[ERROR] Returned from policy update. You may need to reboot your PC"
    $r
    exit
}

"Getting recent Event Log entries from Microsoft-Windows-CodeIntegrity/Operational..."
Sleep -Seconds 1
Get-WinEvent -LogName Microsoft-Windows-CodeIntegrity/Operational -Force -MaxEvents 50 `
    | ?{ $_.TimeCreated -ge $start } `
    | %{ "* $($_.Message)" }

"Verifying enforcement status..."
$dg = (Get-CimInstance -ClassName Win32_DeviceGuard -Namespace root\Microsoft\Windows\DeviceGuard)
if ($dg.CodeIntegrityPolicyEnforcementStatus -eq 0) {
    "* not enforced"
} elseif ($dg.CodeIntegrityPolicyEnforcementStatus -eq 1) {
    "* auditing only"
} elseif ($dg.CodeIntegrityPolicyEnforcementStatus -eq 2) {
    "* enforced"
}