del -EA 0 "${env:SystemRoot}\System32\CodeIntegrity\SiPolicy.bin"
$r = Invoke-CimMethod -Namespace root/microsoft/Windows/CI -ClassName PS_UpdateAndCompareCIPolicy -MethodName Delete

if ($r.ReturnValue) {
    "[ERROR] Returned from policy update. You may need to reboot your PC"
    $r
} else {
    "Policy successfully deleted. You may need to restart your PC"
}
