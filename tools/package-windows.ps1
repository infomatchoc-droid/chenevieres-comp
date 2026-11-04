# Fabrique l'installeur Windows avec l'AAX signé PACE (Chènevières Comp)
# À lancer sur le PC où la clé iLok est branchée, dans le dossier décompressé « Chenevieres-Comp-Windows-kit »
# (artefact de GitHub Actions). Clic droit > « Exécuter avec PowerShell ».
# Il faut Inno Setup 6 (gratuit, jrsoftware.org) installé une fois sur le PC.

$ErrorActionPreference = "Stop"
trap { Write-Host "`nErreur : $_" -ForegroundColor Red; Read-Host "Entrée pour fermer"; exit 1 }
$kit = Split-Path $PSScriptRoot -Parent   # le script est dans tools\, le kit est le dossier au-dessus
$version = (Get-Content (Join-Path $kit "VERSION.txt") -Raw).Trim()
Write-Host "Chenevieres Comp $version : signature de l'AAX puis fabrication de l'installeur`n"

# 1. Signature PACE de l'AAX
$aax = Join-Path $kit "build\ChenevieresComp_artefacts\Release\AAX\Chenevieres Comp.aaxplugin"
& powershell -ExecutionPolicy Bypass -File (Join-Path $kit "tools\sign-aax-windows.ps1") -Bundle $aax
if ($LASTEXITCODE -ne 0) { Write-Host "Signature impossible : installeur non fabriqué." -ForegroundColor Red; Read-Host "Entrée pour fermer"; exit 1 }

# 2. Installeur (Inno Setup)
$iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { Write-Host "Inno Setup 6 introuvable : installez-le depuis jrsoftware.org (gratuit), puis relancez." -ForegroundColor Red; Read-Host "Entrée pour fermer"; exit 1 }
& $iscc "/DAppVersion=$version" (Join-Path $kit "installer\windows\ChenevieresComp.iss")
if ($LASTEXITCODE -ne 0) { Write-Host "Échec d'Inno Setup." -ForegroundColor Red; Read-Host "Entrée pour fermer"; exit 1 }

$exe = Join-Path $kit "dist\Chenevieres-Comp-$version-Windows.exe"
Write-Host "`nPRÊT : $exe" -ForegroundColor Green
Write-Host "C'est ce fichier qu'il faut joindre à la Release (à la place de celui fabriqué par GitHub)."
explorer.exe /select,"$exe"
Read-Host "Entrée pour fermer"
