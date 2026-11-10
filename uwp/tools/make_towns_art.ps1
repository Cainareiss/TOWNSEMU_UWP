# Tsugaru UWP port (FM Towns): regenerates the PS1 art of the frontend as generic FM Towns art.
#   - assets\psx_disc.png        : the disc of the 3D case (1024x1024): a plain CD-ROM label "FM TOWNS"
#   - assets\psx_case_front.png  : jewel case spine/panel texts (Hypnagogia / NTSC U/C / PAL / SLUS) -> "FM TOWNS"
#   - assets\psx_case_back.jpg   : the case back (a PS1 demo's advert) -> a plain "FM TOWNS CD-ROM" back
#   - assets\nocover2.png        : the PlayStation logo and wordmark -> FM TOWNS
#   - assets\nocover-flamenco.png: "A PSI EMULATOR" -> "AN FM TOWNS EMULATOR"
# Plain text only: no Fujitsu / FM TOWNS logos are used. The originals are kept in tools\ps1_art_orig (first run).
param([string]$Assets = 'E:\claude-work\townsuwp\repo\uwp\swanui\assets',
      [string]$Backup = 'E:\claude-work\townsuwp\repo\uwp\tools\ps1_art_orig')
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$files = 'psx_disc.png', 'psx_case_front.png', 'psx_case_back.jpg', 'nocover2.png', 'nocover-flamenco.png'
if (-not (Test-Path $Backup)) {
    New-Item -ItemType Directory -Path $Backup | Out-Null
    foreach ($f in $files) { Copy-Item (Join-Path $Assets $f) $Backup }
}
# always work from the originals, so a second run gives the same result
foreach ($f in $files) { Copy-Item (Join-Path $Backup $f) (Join-Path $Assets $f) -Force }

function Col($a, $r, $g, $b) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Open-Bitmap($path) {
    # load into memory so the file is not locked, and return a 32bpp ARGB copy
    $src = [System.Drawing.Bitmap]::FromFile($path)
    $copy = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gr = [System.Drawing.Graphics]::FromImage($copy)
    $gr.DrawImage($src, 0, 0, $src.Width, $src.Height)   # explicit size: DrawImageUnscaled would apply the file's DPI
    $gr.Dispose(); $src.Dispose()
    return $copy
}
function New-Gfx($bmp) {
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    return $g
}
$center = New-Object System.Drawing.StringFormat
$center.Alignment = 'Center'; $center.LineAlignment = 'Center'
function Fill-Rect($g, $color, $x, $y, $w, $h) {
    $old = $g.CompositingMode
    $g.CompositingMode = 'SourceCopy'
    $br = New-Object System.Drawing.SolidBrush $color
    $g.FillRectangle($br, $x, $y, $w, $h)
    $br.Dispose()
    $g.CompositingMode = $old
}
function Draw-Text($g, $text, $fontName, $size, $style, $color, $cx, $cy) {
    $font = New-Object System.Drawing.Font $fontName, $size, $style, ([System.Drawing.GraphicsUnit]::Pixel)
    $br = New-Object System.Drawing.SolidBrush $color
    $g.DrawString($text, $font, $br, [single]$cx, [single]$cy, $center)
    $br.Dispose(); $font.Dispose()
}
# text with letter spacing, centred on $cx; $cy is the vertical centre
function Draw-Tracked($g, $text, $fontName, $size, $style, $color, $cx, $cy, $tracking) {
    $font = New-Object System.Drawing.Font $fontName, $size, $style, ([System.Drawing.GraphicsUnit]::Pixel)
    $br = New-Object System.Drawing.SolidBrush $color
    $fmt = [System.Drawing.StringFormat]::GenericTypographic
    $widths = @(); $total = 0.0
    foreach ($ch in $text.ToCharArray()) {
        $w = $g.MeasureString([string]$ch, $font, 2000, $fmt).Width
        if ($ch -eq ' ') { $w = $size * 0.32 }
        $widths += $w; $total += $w + $tracking
    }
    $total -= $tracking
    $x = $cx - $total / 2.0
    $i = 0
    foreach ($ch in $text.ToCharArray()) {
        $g.DrawString([string]$ch, $font, $br, [single]$x, [single]($cy - $size * 0.62), $fmt)
        $x += $widths[$i] + $tracking; $i++
    }
    $br.Dispose(); $font.Dispose()
}
function Save-Png($bmp, $path) { $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png) }
function Save-Jpeg($bmp, $path, $quality) {
    $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
    $params = New-Object System.Drawing.Imaging.EncoderParameters 1
    $params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([int64]$quality)
    $rgb = New-Object System.Drawing.Bitmap $bmp.Width, $bmp.Height, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($rgb); $g.DrawImage($bmp, 0, 0, $bmp.Width, $bmp.Height); $g.Dispose()
    $rgb.Save($path, $codec, $params); $rgb.Dispose()
}
function Fill-Circle($g, $brush, $r) { $g.FillEllipse($brush, 512 - $r, 512 - $r, 2 * $r, 2 * $r) }

# ---------------------------------------------------------------- 1. the disc
$disc = New-Object System.Drawing.Bitmap 1024, 1024, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = New-Gfx $disc
$g.Clear((Col 255 26 26 26))
$b = New-Object System.Drawing.SolidBrush (Col 255 206 209 215); Fill-Circle $g $b 460; $b.Dispose()       # rim
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddEllipse(60, 60, 904, 904)                                                                       # printed label
$pg = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
$pg.CenterColor = Col 255 40 104 168
$pg.SurroundColors = @((Col 255 10 26 58))
$g.FillPath($pg, $path); $pg.Dispose(); $path.Dispose()
$pen = New-Object System.Drawing.Pen (Col 150 255 255 255), 3; $g.DrawEllipse($pen, 62, 62, 900, 900); $pen.Dispose()
$b = New-Object System.Drawing.SolidBrush (Col 255 222 225 230); Fill-Circle $g $b 172; $b.Dispose()       # clear hub
$b = New-Object System.Drawing.SolidBrush (Col 255 52 52 58); Fill-Circle $g $b 150; $b.Dispose()
$b = New-Object System.Drawing.SolidBrush (Col 255 236 238 242); Fill-Circle $g $b 64; $b.Dispose()
$b = New-Object System.Drawing.SolidBrush (Col 255 26 26 26); Fill-Circle $g $b 56; $b.Dispose()          # the hole
Draw-Text $g 'FM TOWNS' 'Segoe UI' 112 ([System.Drawing.FontStyle]::Bold) (Col 255 255 255 255) 512 232
Draw-Text $g 'CD-ROM' 'Segoe UI' 64 ([System.Drawing.FontStyle]::Bold) (Col 255 238 196 72) 512 336
Draw-Text $g 'DATA DISC  -  MODE 1 / CD-DA' 'Segoe UI' 34 ([System.Drawing.FontStyle]::Regular) (Col 255 170 196 232) 512 780
Draw-Text $g 'Flamenco One' 'Segoe UI' 40 ([System.Drawing.FontStyle]::Bold) (Col 255 255 90 170) 512 846
$g.Dispose()
Save-Png $disc (Join-Path $Assets 'psx_disc.png'); $disc.Dispose()

# ---------------------------------------------------------------- 2. the case front
function Fix-Spine($g, $w) {
    Fill-Rect $g (Col 255 6 6 6) 1100 10 48 732         # the vertical spine: title, NTSC U/C tags, SLUS serial
    Fill-Rect $g (Col 255 6 6 6) 1170 18 92 114         # the small panel on the right: logo box and PAL label
    $state = $g.Save()
    $g.TranslateTransform(1140, 36)
    $g.RotateTransform(90)
    $font = New-Object System.Drawing.Font 'Segoe UI', 30, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $br = New-Object System.Drawing.SolidBrush (Col 255 245 245 245)
    $g.DrawString('FM TOWNS  CD-ROM', $font, $br, 0, 0)
    $br.Dispose(); $font.Dispose()
    $g.Restore($state)
}
$front = Open-Bitmap (Join-Path $Assets 'psx_case_front.png')
$g = New-Gfx $front; Fix-Spine $g $front.Width; $g.Dispose()
Save-Png $front (Join-Path $Assets 'psx_case_front.png'); $front.Dispose()

# ---------------------------------------------------------------- 3. the case back
$back = Open-Bitmap (Join-Path $Assets 'psx_case_back.jpg')
$g = New-Gfx $back
$rect = New-Object System.Drawing.Rectangle 160, 16, 918, 722
$lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, (Col 255 12 22 46), (Col 255 14 58 104), 90.0
$g.FillRectangle($lg, $rect); $lg.Dispose()
Draw-Text $g 'FM TOWNS' 'Segoe UI' 132 ([System.Drawing.FontStyle]::Bold) (Col 255 255 255 255) 619 270
Draw-Text $g 'CD-ROM' 'Segoe UI' 78 ([System.Drawing.FontStyle]::Bold) (Col 255 238 196 72) 619 400
Draw-Text $g 'Disc image played with Tsugaru (TOWNSEMU)' 'Segoe UI' 34 ([System.Drawing.FontStyle]::Regular) (Col 255 170 196 232) 619 560
Draw-Text $g 'Flamenco One' 'Segoe UI' 40 ([System.Drawing.FontStyle]::Bold) (Col 255 255 90 170) 619 660
Fix-Spine $g $back.Width
$g.Dispose()
Save-Jpeg $back (Join-Path $Assets 'psx_case_back.jpg') 92; $back.Dispose()

# ---------------------------------------------------------------- 4. no cover (PlayStation logo and wordmark)
$nc = Open-Bitmap (Join-Path $Assets 'nocover2.png')
$g = New-Gfx $nc
$bg = Col 255 22 26 38
Fill-Rect $g $bg 2 2 96 122                                   # logo box and its "No Cover" label
Fill-Rect $g (Col 255 0 0 0) 8 8 80 80
Draw-Text $g 'FM' 'Segoe UI' 30 ([System.Drawing.FontStyle]::Bold) (Col 255 255 255 255) 48 34
Draw-Text $g 'TOWNS' 'Segoe UI' 20 ([System.Drawing.FontStyle]::Bold) (Col 255 238 196 72) 48 64
Fill-Rect $g (Col 255 255 255 255) 8 98 80 16
Draw-Text $g 'No Cover' 'Segoe UI' 13 ([System.Drawing.FontStyle]::Bold) (Col 255 0 0 0) 48 106
Fill-Rect $g (Col 255 22 21 26) 0 406 500 94                  # the band with the wordmark and the barcode digits
Draw-Text $g 'FM TOWNS' 'Segoe UI' 62 ([System.Drawing.FontStyle]::Bold) (Col 255 255 255 255) 250 452
$g.Dispose()
Save-Png $nc (Join-Path $Assets 'nocover2.png'); $nc.Dispose()

# ---------------------------------------------------------------- 5. the Flamenco One no-cover card
$nf = Open-Bitmap (Join-Path $Assets 'nocover-flamenco.png')
$g = New-Gfx $nf
Fill-Rect $g (Col 255 22 26 38) 80 304 340 36                 # "A PSI EMULATOR"
Draw-Tracked $g 'AN FM TOWNS EMULATOR' 'Segoe UI' 22 ([System.Drawing.FontStyle]::Bold) (Col 255 252 47 150) 250 322 4.5
$g.Dispose()
Save-Png $nf (Join-Path $Assets 'nocover-flamenco.png'); $nf.Dispose()
Write-Host 'FM Towns art written to' $Assets
