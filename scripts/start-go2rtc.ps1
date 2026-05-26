$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "tools\go2rtc\go2rtc.exe"
$config = Join-Path $root "config\go2rtc.yaml"

if (-not (Test-Path -LiteralPath $exe)) {
  throw "No se encontro go2rtc.exe en $exe"
}

if (-not (Test-Path -LiteralPath $config)) {
  throw "No se encontro la configuracion en $config"
}

& $exe -config $config
