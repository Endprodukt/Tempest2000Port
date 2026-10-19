param([string]$Out, [string]$Keys = "", [string]$ExeArgs = "--windowed")
# Start the game, optionally post keys to its window, capture only that window.
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
  public struct RECT { public int L, T, R, B; }
}
"@
Add-Type -AssemblyName System.Drawing
$exe = "F:\Tempest 2000 Port\build\pc\tempest2000.exe"
$p = Start-Process -FilePath $exe -ArgumentList $ExeArgs -PassThru -RedirectStandardError "$Out.err.txt"
Start-Sleep -Milliseconds 2500
$p.Refresh()
$h = $p.MainWindowHandle
function Snap($path) {
  $r = New-Object W+RECT
  [W]::GetClientRect($h, [ref]$r) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap ($r.R), ($r.B)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $dc = $g.GetHdc()
  [W]::PrintWindow($h, $dc, 3) | Out-Null
  $g.ReleaseHdc($dc)
  $bmp.Save($path)
  $g.Dispose(); $bmp.Dispose()
}
$n = 0
foreach ($k in ($Keys -split ",")) {
  if ($k -eq "") { continue }
  if ($k -eq "snap") { $n++; Snap("$Out.$n.png"); continue }
  if ($k -match "^w(\d+)$") { Start-Sleep -Milliseconds ([int]$Matches[1]); continue }
  $up = $false; $down = $true
  if ($k.StartsWith("+")) { $k = $k.Substring(1) } elseif ($k.StartsWith("-")) { $k = $k.Substring(1); $up = $true; $down = $false } else { $up = $true }
  $vk = [uint32]("0x" + $k)
  $sc = [W]::MapVirtualKey($vk, 0)
  $ext = 0; if ($vk -ge 0x25 -and $vk -le 0x28) { $ext = 1 -shl 24 }
  if ($down) { [W]::PostMessage($h, 0x100, [IntPtr]$vk, [IntPtr](1 -bor ($sc -shl 16) -bor $ext)) | Out-Null; Start-Sleep -Milliseconds 60 }
  if ($up) { [W]::PostMessage($h, 0x101, [IntPtr]$vk, [IntPtr](1 -bor ($sc -shl 16) -bor $ext -bor 0xC0000000)) | Out-Null; Start-Sleep -Milliseconds 60 }
}
Snap("$Out.png")
Stop-Process -Id $p.Id -Force
Write-Output "handle=$h"
