# Signature PACE de l'AAX Windows (Chènevières Comp)
# A lancer sur le PC où la clé iLok (licences PACE) est branchée.
# Clic droit sur ce fichier > « Exécuter avec PowerShell », ou :  powershell -ExecutionPolicy Bypass -File sign-aax-windows.ps1
#
# Réglages : compte iLok, empreinte du certificat (signid) et configuration PACE (wcguid).
# Aucun mot de passe n'est écrit dans ce fichier : il est demandé à chaque lancement.

param([string]$Bundle = "")   # dossier « Chenevieres Comp.aaxplugin » ; vide = fenêtre de choix

$Account = "infomatchoc@gmail.com"                       # compte iLok (commande donnée par PACE Central)
$SignId  = ""                                            # empreinte (Thumbprint) d'un certificat acheté ; vide = certificat maison créé automatiquement
$WcGuid  = "DCD91EF0-CE23-11F1-898A-00505692C25A"        # Wrap Configuration « Chenevieres Comp AAX » (PACE Central)

$ErrorActionPreference = "Stop"

# 1. wraptool
$wraptool = Get-ChildItem -Path "${env:ProgramFiles(x86)}\PACEAntiPiracy", "$env:ProgramFiles\PACEAntiPiracy" -Recurse -Filter wraptool.exe -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
if (-not $wraptool) { Write-Host "wraptool.exe introuvable : installez d'abord PACECodeSigningForAAXSDKWindows." -ForegroundColor Red; Read-Host "Entrée pour fermer"; exit 1 }
Write-Host "wraptool : $($wraptool.FullName)"

# 1 bis. Certificat de signature de code
#   Clé « CryptoAPI » classique (KeySpec Signature) : wraptool ne sait pas utiliser les clés CNG.
#   Le certificat est aussi déclaré de confiance pour l'utilisateur (Racines + Éditeurs approuvés).
if (-not $SignId) {
    $subject = "CN=Studio des Chenevieres (AAX 2)"
    Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq "CN=Studio des Chenevieres (AAX)" } | Remove-Item -ErrorAction SilentlyContinue
    $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Where-Object { $_.Subject -eq $subject -and $_.NotAfter -gt (Get-Date) } | Select-Object -First 1
    if (-not $cert) {
        Write-Host "Création du certificat maison « $subject » (valable 5 ans)..."
        $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $subject -CertStoreLocation Cert:\CurrentUser\My `
                -Provider "Microsoft Enhanced RSA and AES Cryptographic Provider" -KeySpec Signature -KeyLength 2048 `
                -HashAlgorithm SHA256 -KeyExportPolicy NonExportable -NotAfter (Get-Date).AddYears(5)
    }
    $SignId = $cert.Thumbprint
    foreach ($store in "Root", "TrustedPublisher") {
        if (-not (Get-ChildItem "Cert:\CurrentUser\$store" | Where-Object { $_.Thumbprint -eq $SignId })) {
            if ($store -eq "Root") { Write-Host "Windows va demander de confirmer l'installation du certificat : répondez OUI." -ForegroundColor Yellow }
            $tmp = Join-Path $env:TEMP "chnv-aax.cer"
            Export-Certificate -Cert $cert -FilePath $tmp | Out-Null
            Import-Certificate -FilePath $tmp -CertStoreLocation "Cert:\CurrentUser\$store" | Out-Null
            Remove-Item $tmp
        }
    }
}
Write-Host "Certificat (signid) : $SignId"

# 2. Le plugin AAX à signer (dossier Chenevieres Comp.aaxplugin)
if ($Bundle) { $bundle = $Bundle } else {
    Add-Type -AssemblyName System.Windows.Forms
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = "Choisissez le dossier « Chenevieres Comp.aaxplugin » à signer"
    if ($dlg.ShowDialog() -ne "OK") { exit 1 }
    $bundle = $dlg.SelectedPath
}
$binary = Join-Path $bundle "Contents\x64\Chenevieres Comp.aaxplugin"
if (-not (Test-Path $binary)) { Write-Host "Fichier introuvable : $binary" -ForegroundColor Red; Read-Host "Entrée pour fermer"; exit 1 }

# 3. Signature
$pw = Read-Host "Mot de passe iLok (laisser vide s'il a déjà été mémorisé par wraptool)" -AsSecureString
$plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($pw))
$wtArgs = @("sign", "--verbose", "--account", $Account, "--wcguid", $WcGuid, "--signid", $SignId,
          "--in", $binary, "--out", $binary, "--autoinstall", "on")
if ($plain) { $wtArgs += @("--password", $plain) }
& $wraptool.FullName @wtArgs
$code = $LASTEXITCODE
$plain = $null

# 4. Vérification
if ($code -eq 0) {
    & $wraptool.FullName verify --verbose --in $binary
    Write-Host "`nSIGNÉ : $binary" -ForegroundColor Green
} else {
    Write-Host "`nÉchec de la signature (code $code). Envoyez une capture de ce texte (sans mot de passe)." -ForegroundColor Red
}
if (-not $Bundle) { Read-Host "Entrée pour fermer" }
exit $code
