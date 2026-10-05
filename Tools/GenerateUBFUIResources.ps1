$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$output = Join-Path (Split-Path $PSScriptRoot -Parent) 'Content\Presentation\UIArt'
New-Item -ItemType Directory -Path $output -Force | Out-Null

function New-Canvas([int]$Width, [int]$Height) {
    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)
    return @{ Bitmap = $bitmap; Graphics = $graphics }
}

function Save-Canvas($Canvas, [string]$Name) {
    $Canvas.Bitmap.Save((Join-Path $output $Name), [System.Drawing.Imaging.ImageFormat]::Png)
    $Canvas.Graphics.Dispose()
    $Canvas.Bitmap.Dispose()
}

function Draw-Frame([string]$Name, [System.Drawing.Color]$Tint) {
    $canvas = New-Canvas 256 256
    $g = $canvas.Graphics
    $fill = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(104, 8, 13, 22))
    $soft = [System.Drawing.Color]::FromArgb(38, $Tint.R, $Tint.G, $Tint.B)
    $mid = [System.Drawing.Color]::FromArgb(115, $Tint.R, $Tint.G, $Tint.B)
    $bright = [System.Drawing.Color]::FromArgb(205, $Tint.R, $Tint.G, $Tint.B)
    $pSoft = [System.Drawing.Pen]::new($soft, 8)
    $pMid = [System.Drawing.Pen]::new($mid, 2)
    $pBright = [System.Drawing.Pen]::new($bright, 1)
    $g.FillRectangle($fill, 16, 16, 224, 224)
    foreach ($pen in @($pSoft, $pMid, $pBright)) {
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Square
        $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Square
    }
    $g.DrawRectangle($pSoft, 7, 7, 242, 242)
    $g.DrawRectangle($pMid, 14, 14, 228, 228)
    $g.DrawRectangle($pBright, 15, 15, 226, 226)
    $length = 42
    $inset = 20
    $g.DrawLines($pBright, [System.Drawing.Point[]]@([System.Drawing.Point]::new($inset, $inset + $length), [System.Drawing.Point]::new($inset, $inset), [System.Drawing.Point]::new($inset + $length, $inset)))
    $g.DrawLines($pBright, [System.Drawing.Point[]]@([System.Drawing.Point]::new(256 - $inset - $length, $inset), [System.Drawing.Point]::new(256 - $inset, $inset), [System.Drawing.Point]::new(256 - $inset, $inset + $length)))
    $g.DrawLines($pBright, [System.Drawing.Point[]]@([System.Drawing.Point]::new($inset, 256 - $inset - $length), [System.Drawing.Point]::new($inset, 256 - $inset), [System.Drawing.Point]::new($inset + $length, 256 - $inset)))
    $g.DrawLines($pBright, [System.Drawing.Point[]]@([System.Drawing.Point]::new(256 - $inset - $length, 256 - $inset), [System.Drawing.Point]::new(256 - $inset, 256 - $inset), [System.Drawing.Point]::new(256 - $inset, 256 - $inset - $length)))
    $diamond = [System.Drawing.Point[]]@([System.Drawing.Point]::new(128, 13), [System.Drawing.Point]::new(135, 20), [System.Drawing.Point]::new(128, 27), [System.Drawing.Point]::new(121, 20))
    $g.DrawPolygon($pBright, $diamond)
    $g.DrawLine($pMid, 36, 21, 105, 21)
    $g.DrawLine($pMid, 151, 21, 220, 21)
    $g.DrawLine($pMid, 36, 235, 105, 235)
    $g.DrawLine($pMid, 151, 235, 220, 235)
    $fill.Dispose(); $pSoft.Dispose(); $pMid.Dispose(); $pBright.Dispose()
    Save-Canvas $canvas $Name
}

function New-Icon([string]$Name, [string]$Kind, [System.Drawing.Color]$Tint) {
    $canvas = New-Canvas 128 128
    $g = $canvas.Graphics
    $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(232, $Tint.R, $Tint.G, $Tint.B), 5)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $thin = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(150, $Tint.R, $Tint.G, $Tint.B), 2)
    switch ($Kind) {
        'armor' {
            $g.DrawPolygon($pen, [System.Drawing.Point[]]@([System.Drawing.Point]::new(64, 12), [System.Drawing.Point]::new(103, 27), [System.Drawing.Point]::new(97, 76), [System.Drawing.Point]::new(64, 111), [System.Drawing.Point]::new(31, 76), [System.Drawing.Point]::new(25, 27), [System.Drawing.Point]::new(64, 12)))
            $g.DrawLine($thin, 64, 28, 64, 93); $g.DrawLine($thin, 42, 45, 64, 55); $g.DrawLine($thin, 86, 45, 64, 55)
        }
        'weapon' {
            $g.DrawLine($pen, 31, 96, 94, 31); $g.DrawLine($pen, 26, 77, 50, 101); $g.DrawLine($pen, 19, 105, 36, 88)
            $g.DrawPolygon($thin, [System.Drawing.Point[]]@([System.Drawing.Point]::new(94, 22), [System.Drawing.Point]::new(105, 16), [System.Drawing.Point]::new(99, 32), [System.Drawing.Point]::new(94, 22)))
        }
        'accessory' {
            $g.DrawEllipse($pen, 25, 32, 78, 74); $g.DrawPolygon($pen, [System.Drawing.Point[]]@([System.Drawing.Point]::new(64, 15), [System.Drawing.Point]::new(78, 31), [System.Drawing.Point]::new(64, 46), [System.Drawing.Point]::new(50, 31), [System.Drawing.Point]::new(64, 15)))
            $g.DrawLine($thin, 44, 88, 84, 88)
        }
        'character' {
            $g.DrawEllipse($pen, 45, 20, 38, 38); $g.DrawArc($pen, 24, 48, 80, 67, 190, 160); $g.DrawLine($thin, 64, 65, 64, 106)
        }
        'shop' {
            $g.DrawRectangle($pen, 26, 42, 76, 65); $g.DrawArc($pen, 41, 16, 46, 53, 180, 180); $g.DrawLine($thin, 26, 61, 102, 61); $g.DrawLine($thin, 51, 72, 51, 94); $g.DrawLine($thin, 77, 72, 77, 94)
        }
        'q' {
            $g.DrawLine($pen, 27, 97, 91, 33); $g.DrawLine($pen, 27, 78, 46, 97); $g.DrawLine($pen, 20, 105, 38, 87); $g.DrawPolygon($thin, [System.Drawing.Point[]]@([System.Drawing.Point]::new(91, 24), [System.Drawing.Point]::new(105, 17), [System.Drawing.Point]::new(98, 34), [System.Drawing.Point]::new(91, 24)))
        }
        'e' {
            $g.DrawArc($pen, 20, 20, 88, 88, 38, 288); $g.DrawPolygon($pen, [System.Drawing.Point[]]@([System.Drawing.Point]::new(91, 22), [System.Drawing.Point]::new(108, 18), [System.Drawing.Point]::new(102, 36), [System.Drawing.Point]::new(91, 22)))
            $g.DrawEllipse($thin, 52, 52, 24, 24)
        }
        'ultimate' {
            $pts = [System.Drawing.Point[]]::new(16)
            for ($i = 0; $i -lt 16; $i++) { $angle = (-[Math]::PI / 2) + ($i * [Math]::PI / 8); $radius = if (($i % 2) -eq 0) { 48 } else { 22 }; $pts[$i] = [System.Drawing.Point]::new([int](64 + [Math]::Cos($angle) * $radius), [int](64 + [Math]::Sin($angle) * $radius)) }
            $g.DrawPolygon($pen, $pts); $g.DrawEllipse($thin, 50, 50, 28, 28)
        }
        'passive' {
            $g.DrawPolygon($pen, [System.Drawing.Point[]]@([System.Drawing.Point]::new(64, 13), [System.Drawing.Point]::new(108, 64), [System.Drawing.Point]::new(64, 115), [System.Drawing.Point]::new(20, 64), [System.Drawing.Point]::new(64, 13)))
            $g.DrawLine($thin, 64, 31, 64, 97); $g.DrawLine($thin, 38, 64, 90, 64); $g.DrawEllipse($thin, 54, 54, 20, 20)
        }
    }
    $pen.Dispose(); $thin.Dispose()
    Save-Canvas $canvas $Name
}

Draw-Frame 'panel-frame-gold.png' ([System.Drawing.Color]::FromArgb(223, 177, 91))
Draw-Frame 'panel-frame-red.png' ([System.Drawing.Color]::FromArgb(226, 62, 79))
Draw-Frame 'panel-frame-blue.png' ([System.Drawing.Color]::FromArgb(76, 166, 233))

$gold = [System.Drawing.Color]::FromArgb(231, 194, 113)
$ice = [System.Drawing.Color]::FromArgb(218, 231, 244)
New-Icon 'icon-armor.png' 'armor' $gold
New-Icon 'icon-weapon.png' 'weapon' $ice
New-Icon 'icon-accessory.png' 'accessory' $gold
New-Icon 'icon-character.png' 'character' $ice
New-Icon 'icon-shop.png' 'shop' $gold
New-Icon 'skill-q.png' 'q' ([System.Drawing.Color]::FromArgb(113, 210, 241))
New-Icon 'skill-e.png' 'e' ([System.Drawing.Color]::FromArgb(240, 122, 126))
New-Icon 'skill-ultimate.png' 'ultimate' $gold
New-Icon 'skill-passive.png' 'passive' ([System.Drawing.Color]::FromArgb(195, 202, 221))

Get-ChildItem -LiteralPath $output -Filter '*.png' | Select-Object Name, Length
