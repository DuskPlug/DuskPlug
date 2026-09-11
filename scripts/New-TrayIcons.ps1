#Requires -Version 5.1
# One-time helper to generate tray lightbulb icons.

Add-Type -AssemblyName System.Drawing

function New-LightbulbBitmap {
    param(
        [int]$Size,
        [System.Drawing.Color]$BulbColor,
        [System.Drawing.Color]$OutlineColor
    )

    $bmp = New-Object System.Drawing.Bitmap $Size, $Size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $penWidth = [Math]::Max(1, [int]($Size / 16))
    $outline = New-Object System.Drawing.Pen($OutlineColor, $penWidth)
    $fill = New-Object System.Drawing.SolidBrush($BulbColor)

    $cx = $Size / 2
    $bulbW = $Size * 0.55
    $bulbH = $Size * 0.42
    $bulbRect = [System.Drawing.RectangleF]::new($cx - $bulbW / 2, $Size * 0.12, $bulbW, $bulbH)
    $g.FillEllipse($fill, $bulbRect)
    $g.DrawEllipse($outline, $bulbRect)

    $baseW = $Size * 0.28
    $baseH = $Size * 0.14
    $baseRect = [System.Drawing.RectangleF]::new($cx - $baseW / 2, $Size * 0.52, $baseW, $baseH)
    $g.FillRectangle($fill, $baseRect)
    $g.DrawRectangle($outline, $baseRect.X, $baseRect.Y, $baseRect.Width, $baseRect.Height)

    $pinW = $Size * 0.16
    $pinH = $Size * 0.12
    $pinRect = [System.Drawing.RectangleF]::new($cx - $pinW / 2, $Size * 0.66, $pinW, $pinH)
    $g.FillRectangle($fill, $pinRect)
    $g.DrawRectangle($outline, $pinRect.X, $pinRect.Y, $pinRect.Width, $pinRect.Height)

    if ($BulbColor.R -gt 200) {
        $glow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(90, 255, 230, 120))
        $g.FillEllipse($glow, ($cx - $bulbW * 0.35), ($Size * 0.18), ($bulbW * 0.35), ($bulbH * 0.35))
    }

    $outline.Dispose()
    $fill.Dispose()
    $g.Dispose()
    return $bmp
}

function Save-BitmapAsIcon {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [string]$Path
    )

    $iconHandle = $Bitmap.GetHicon()
    try {
        $icon = [System.Drawing.Icon]::FromHandle($iconHandle)
        $stream = [System.IO.File]::Create($Path)
        $icon.Save($stream)
        $stream.Close()
        $icon.Dispose()
    } finally {
        $Bitmap.Dispose()
    }
}

function New-SmartLightbulbBitmap {
    param(
        [int]$Size,
        [System.Drawing.Color]$BulbColor,
        [System.Drawing.Color]$OutlineColor
    )

    $bmp = New-LightbulbBitmap -Size $Size -BulbColor $BulbColor -OutlineColor $OutlineColor
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    $cx = $Size / 2
    $ringPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 80, 160, 255)), ([Math]::Max(1, [int]($Size / 14)))
    $ringRadius = $Size * 0.46
    $g.DrawEllipse($ringPen, ($cx - $ringRadius), ($cx - $ringRadius), ($ringRadius * 2), ($ringRadius * 2))
    $ringPen.Dispose()
    $g.Dispose()
    return $bmp
}

$assetsDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'assets'
New-Item -ItemType Directory -Path $assetsDir -Force | Out-Null

$onBmp = New-LightbulbBitmap -Size 32 -BulbColor ([System.Drawing.Color]::FromArgb(255, 255, 210, 80)) -OutlineColor ([System.Drawing.Color]::FromArgb(255, 180, 130, 20))
Save-BitmapAsIcon -Bitmap $onBmp -Path (Join-Path $assetsDir 'light-on.ico')

$offBmp = New-LightbulbBitmap -Size 32 -BulbColor ([System.Drawing.Color]::FromArgb(255, 110, 110, 110)) -OutlineColor ([System.Drawing.Color]::FromArgb(255, 70, 70, 70))
Save-BitmapAsIcon -Bitmap $offBmp -Path (Join-Path $assetsDir 'light-off.ico')

$appBmp = New-LightbulbBitmap -Size 256 -BulbColor ([System.Drawing.Color]::FromArgb(255, 255, 210, 80)) -OutlineColor ([System.Drawing.Color]::FromArgb(255, 180, 130, 20))
Save-BitmapAsIcon -Bitmap $appBmp -Path (Join-Path $assetsDir 'app.ico')

$onColor = [System.Drawing.Color]::FromArgb(255, 255, 210, 80)
$onOutline = [System.Drawing.Color]::FromArgb(255, 180, 130, 20)
$offColor = [System.Drawing.Color]::FromArgb(255, 110, 110, 110)
$offOutline = [System.Drawing.Color]::FromArgb(255, 70, 70, 70)

$smartOnBmp = New-SmartLightbulbBitmap -Size 32 -BulbColor $onColor -OutlineColor $onOutline
Save-BitmapAsIcon -Bitmap $smartOnBmp -Path (Join-Path $assetsDir 'light-smart-on.ico')

$smartOffBmp = New-SmartLightbulbBitmap -Size 32 -BulbColor $offColor -OutlineColor $offOutline
Save-BitmapAsIcon -Bitmap $smartOffBmp -Path (Join-Path $assetsDir 'light-smart-off.ico')

$smartBmp = New-SmartLightbulbBitmap -Size 32 -BulbColor $onColor -OutlineColor $onOutline
Save-BitmapAsIcon -Bitmap $smartBmp -Path (Join-Path $assetsDir 'light-smart.ico')

Write-Host "Created icons in $assetsDir"
