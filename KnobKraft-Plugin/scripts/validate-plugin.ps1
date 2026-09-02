param(
	[Parameter(Mandatory = $true)]
	[string] $PluginValPath,
	[Parameter(Mandatory = $true)]
	[string] $Vst3Path,
	[ValidateRange(1, 10)]
	[int] $Strictness = 10
)

$validator = (Resolve-Path -LiteralPath $PluginValPath -ErrorAction Stop).Path
$plugin = (Resolve-Path -LiteralPath $Vst3Path -ErrorAction Stop).Path

& $validator --validate-in-process --strictness-level $Strictness --validate $plugin
exit $LASTEXITCODE
