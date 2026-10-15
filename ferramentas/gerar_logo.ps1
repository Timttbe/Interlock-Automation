# Gera src/logo.h a partir de ferramentas/WiresIcon.png: duas versões pequenas da logo (64 px de
# altura, nítida em telas de alta resolução), embutidas no firmware.
#   - tema claro: as cores originais;
#   - tema escuro: o preto da logo ("WI" e o escudo) vira quase branco, o amarelo fica igual.
# A imagem original precisa ter fundo transparente.
#
# Uso (PowerShell, na pasta do projeto):  powershell -ExecutionPolicy Bypass -File ferramentas\gerar_logo.ps1

param(
  [string]$Entrada = (Join-Path $PSScriptRoot "WiresIcon.png"),
  [string]$Saida = (Join-Path $PSScriptRoot "..\src\logo.h"),
  [int]$Altura = 64
)

Add-Type -AssemblyName System.Drawing

function Redimensionar([System.Drawing.Image]$origem, [int]$altura) {
  $largura = [int][math]::Round($origem.Width * $altura / $origem.Height)
  $bmp = New-Object System.Drawing.Bitmap $largura, $altura, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.DrawImage($origem, 0, 0, $largura, $altura)
  $g.Dispose()
  return $bmp
}

# Pixels neutros e escuros (o preto da logo, inclusive as bordas suavizadas) viram claros.
function ParaTemaEscuro([System.Drawing.Bitmap]$bmp) {
  $escuro = $bmp.Clone()
  for ($y = 0; $y -lt $escuro.Height; $y++) {
    for ($x = 0; $x -lt $escuro.Width; $x++) {
      $c = $escuro.GetPixel($x, $y)
      if ($c.A -eq 0) { continue }
      $max = [math]::Max($c.R, [math]::Max($c.G, $c.B))
      $min = [math]::Min($c.R, [math]::Min($c.G, $c.B))
      if (($max - $min) -lt 60 -and $max -lt 160) {
        $escuro.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($c.A, 235, 238, 242))
      }
    }
  }
  return $escuro
}

function BytesPng([System.Drawing.Bitmap]$bmp) {
  $ms = New-Object System.IO.MemoryStream
  $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  return , $ms.ToArray()
}

function ArrayC([string]$nome, [byte[]]$bytes) {
  $sb = New-Object System.Text.StringBuilder
  [void]$sb.AppendLine("static const uint8_t $nome[] = {")
  for ($i = 0; $i -lt $bytes.Length; $i += 20) {
    $fim = [math]::Min($i + 20, $bytes.Length) - 1
    $linha = ($bytes[$i..$fim] | ForEach-Object { "0x{0:x2}" -f $_ }) -join ","
    [void]$sb.AppendLine("  $linha,")
  }
  [void]$sb.AppendLine("};")
  return $sb.ToString()
}

$original = [System.Drawing.Image]::FromFile((Resolve-Path $Entrada))
$claro = Redimensionar $original $Altura
$original.Dispose()
$escuro = ParaTemaEscuro $claro
$bytesClaro = BytesPng $claro
$bytesEscuro = BytesPng $escuro

$texto = "#pragma once`r`n" +
  "// Logo da página, gerada por ferramentas/gerar_logo.ps1 a partir de ferramentas/WiresIcon.png.`r`n" +
  "// Não edite à mão: troque a imagem e rode o script de novo.`r`n" +
  "// $($claro.Width) x $($claro.Height) px.`r`n`r`n" +
  "#include <stdint.h>`r`n`r`n" +
  (ArrayC "LOGO_CLARO" $bytesClaro) + "`r`n" +
  (ArrayC "LOGO_ESCURO" $bytesEscuro)
[System.IO.File]::WriteAllText((Join-Path (Resolve-Path (Split-Path $Saida)) (Split-Path $Saida -Leaf)), $texto, (New-Object System.Text.UTF8Encoding $false))

"Logo: $($claro.Width) x $($claro.Height) px; tema claro $($bytesClaro.Length) bytes, tema escuro $($bytesEscuro.Length) bytes"
$claro.Dispose()
$escuro.Dispose()
