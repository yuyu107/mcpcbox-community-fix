Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

function Pick-File($title, $filter) {
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = $title
    $dialog.Filter = $filter
    if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return $null }
    return $dialog.FileName
}

function Copy-StretchedFace($source, $target, $sx, $sy, $sw, $sh, $dx, $dy, $dw, $dh) {
    # Nearest-neighbour expansion is required for pixel-art skins.  In
    # particular, it converts a 3-pixel Alex arm face into the 4-pixel Steve
    # face expected by MCPCBox's old preview and offline skin installer.
    for ($y = 0; $y -lt $dh; $y++) {
        $sourceY = $sy + [Math]::Min($sh - 1, [Math]::Floor($y * $sh / $dh))
        for ($x = 0; $x -lt $dw; $x++) {
            $sourceX = $sx + [Math]::Min($sw - 1, [Math]::Floor($x * $sw / $dw))
            $target.SetPixel($dx + $x, $dy + $y, $source.GetPixel($sourceX, $sourceY))
        }
    }
}

function Copy-StretchedFaceFlippedX($source, $target, $sx, $sy, $sw, $sh, $dx, $dy, $dw, $dh) {
    # Used only for arm bottom faces after exchanging the left/right arm UV
    # blocks. Moving a bottom face to the opposite limb reverses its local X
    # direction, so sample the source face from right to left.
    for ($y = 0; $y -lt $dh; $y++) {
        $sourceY = $sy + [Math]::Min($sh - 1, [Math]::Floor($y * $sh / $dh))
        for ($x = 0; $x -lt $dw; $x++) {
            $sampleX = [Math]::Min($sw - 1, [Math]::Floor($x * $sw / $dw))
            $sourceX = $sx + ($sw - 1 - $sampleX)
            $target.SetPixel($dx + $x, $dy + $y, $source.GetPixel($sourceX, $sourceY))
        }
    }
}

function Test-SlimSkin($bitmap) {
    if ($bitmap.Width -ne 64 -or $bitmap.Height -ne 64) { return $false }
    # Mojang's slim layout leaves these two columns unused on the right arm.
    for ($y = 20; $y -lt 32; $y++) {
        if ($bitmap.GetPixel(54, $y).A -ne 0 -or $bitmap.GetPixel(55, $y).A -ne 0) {
            return $false
        }
    }
    return $true
}

function Normalize-LegacyPng($sourcePath, $destinationPath, $convertSlim) {
    $source = New-Object System.Drawing.Bitmap($sourcePath)
    try {
        $target = New-Object System.Drawing.Bitmap($source.Width, $source.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $source.Height; $y++) {
                for ($x = 0; $x -lt $source.Width; $x++) {
                    $pixel = $source.GetPixel($x, $y)
                    if ($pixel.A -eq 0) {
                        $target.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                    } else {
                        # The legacy MCPCBox renderer mishandles partial alpha
                        # and can turn an entire overlay face black. Minecraft
                        # skins are pixel art, so retain RGB and make every
                        # visible pixel fully opaque.
                        $target.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $pixel.R, $pixel.G, $pixel.B))
                    }
                }
            }
            if ($source.Width -eq 64 -and $source.Height -eq 64) {
                # MCPCBox's legacy preview reads the rear face of the torso
                # overlay with the wrong UV mapping on some modern skins.
                # Pre-compose only that 8x12 face onto the base torso back,
                # then clear the overlay face. Other faces remain untouched.
                for ($y = 0; $y -lt 12; $y++) {
                    for ($x = 0; $x -lt 8; $x++) {
                        $overlay = $source.GetPixel(32 + $x, 36 + $y)
                        if ($overlay.A -ne 0) {
                            $target.SetPixel(
                                32 + $x,
                                20 + $y,
                                [System.Drawing.Color]::FromArgb(255, $overlay.R, $overlay.G, $overlay.B)
                            )
                        }
                        $target.SetPixel(32 + $x, 36 + $y, [System.Drawing.Color]::Transparent)
                    }
                }
                if ($convertSlim) {
                    # Expand only the two base-layer arm blocks from the slim
                    # 3-pixel layout to the classic 4-pixel layout expected by
                    # MCPCBox. Do not clear whole blocks and do not touch either
                    # arm overlay; those broad edits caused regressions before.

                    # MCPCBox binds the post-1.8 arm blocks to the opposite
                    # sides. Put the source left arm into its right-arm target.
                    Copy-StretchedFace $source $target 36 48 3 4 44 16 4 4
                    Copy-StretchedFaceFlippedX $source $target 39 48 3 4 48 16 4 4
                    # Swapping limbs also exchanges the inner and outer sides.
                    Copy-StretchedFace $source $target 39 52 4 12 40 20 4 12
                    Copy-StretchedFace $source $target 36 52 3 12 44 20 4 12
                    Copy-StretchedFace $source $target 32 52 4 12 48 20 4 12
                    Copy-StretchedFace $source $target 43 52 3 12 52 20 4 12

                    # Put the source right arm into its left-arm target.
                    Copy-StretchedFace $source $target 44 16 3 4 36 48 4 4
                    Copy-StretchedFaceFlippedX $source $target 47 16 3 4 40 48 4 4
                    # Swapping limbs also exchanges the inner and outer sides.
                    Copy-StretchedFace $source $target 47 20 4 12 32 52 4 12
                    Copy-StretchedFace $source $target 44 20 3 12 36 52 4 12
                    Copy-StretchedFace $source $target 40 20 4 12 40 52 4 12
                    Copy-StretchedFace $source $target 51 20 3 12 44 52 4 12

                    # Every face in the base arm layer must be opaque after a
                    # slim-to-classic conversion. Abort instead of silently
                    # saving a skin with the two transparent rear seams.
                    foreach ($face in @(
                        @(44,16,4,4), @(48,16,4,4), @(40,20,4,12),
                        @(44,20,4,12), @(48,20,4,12), @(52,20,4,12),
                        @(36,48,4,4), @(40,48,4,4), @(32,52,4,12),
                        @(36,52,4,12), @(40,52,4,12), @(44,52,4,12)
                    )) {
                        for ($fy = 0; $fy -lt $face[3]; $fy++) {
                            for ($fx = 0; $fx -lt $face[2]; $fx++) {
                                if ($target.GetPixel($face[0] + $fx, $face[1] + $fy).A -eq 0) {
                                    throw "纤细手臂转换后仍存在透明像素。"
                                }
                            }
                        }
                    }
                }
            }
            $target.Save($destinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $target.Dispose() }
    } finally { $source.Dispose() }
}

function Convert-ToLegacy32($sourcePath, $destinationPath) {
    $source = New-Object System.Drawing.Bitmap($sourcePath)
    try {
        $target = New-Object System.Drawing.Bitmap(64, 32, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            # Start with the legacy-compatible upper half: head/hat plus the
            # base torso, right arm and right leg UV blocks.
            for ($y = 0; $y -lt 32; $y++) {
                for ($x = 0; $x -lt 64; $x++) {
                    $pixel = $source.GetPixel($x, $y)
                    if ($pixel.A -eq 0) {
                        $target.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                    } else {
                        $target.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $pixel.R, $pixel.G, $pixel.B))
                    }
                }
            }

            if ($source.Height -eq 64) {
                # 1.7 has no body/arm/leg second layer. Pre-compose the modern
                # overlays onto their matching legacy base blocks. The head hat
                # layer already lives in the upper half and remains separate.
                foreach ($region in @(
                    @(16,32,16,16,24),  # torso overlay -> torso base
                    @(40,32,40,16,16),  # right-arm overlay -> right-arm base
                    @(0,32,0,16,16)     # right-leg overlay -> right-leg base
                )) {
                    for ($ry = 0; $ry -lt 16; $ry++) {
                        for ($rx = 0; $rx -lt $region[4]; $rx++) {
                            $overlay = $source.GetPixel($region[0] + $rx, $region[1] + $ry)
                            if ($overlay.A -ne 0) {
                                $target.SetPixel(
                                    $region[2] + $rx,
                                    $region[3] + $ry,
                                    [System.Drawing.Color]::FromArgb(255, $overlay.R, $overlay.G, $overlay.B)
                                )
                            }
                        }
                    }
                }
            }
            $target.Save($destinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $target.Dispose() }
    } finally { $source.Dispose() }
}

function Add-SkinEntry($doc, $entryId, $entryName, $entryPath) {
    $existing = $doc.SelectSingleNode("/download_res/res[@id='$entryId']")
    if ($existing) { [void]$doc.DocumentElement.RemoveChild($existing) }
    $entry = $doc.CreateElement("res")
    $entry.SetAttribute("type", "2")
    $entry.SetAttribute("id", [string]$entryId)
    $entry.SetAttribute("name", $entryName)
    $entry.SetAttribute("file_path", $entryPath)
    $entry.SetAttribute("completed_time", (Get-Date -Format "yyyy/M/d"))
    $entry.SetAttribute("file_size", [string](Get-Item -LiteralPath $entryPath).Length)
    $storedHash = (Get-FileHash -Algorithm MD5 -LiteralPath $entryPath).Hash.ToLowerInvariant()
    $entry.SetAttribute("md5", $storedHash)
    [void]$doc.DocumentElement.AppendChild($entry)
}

function Convert-SlimToClassic($sourcePath, $destinationPath) {
    $source = New-Object System.Drawing.Bitmap($sourcePath)
    try {
        $target = New-Object System.Drawing.Bitmap(64, 64, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($target)
            try {
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.DrawImageUnscaled($source, 0, 0)
            } finally { $graphics.Dispose() }

            # Clear the four arm UV blocks before rebuilding them in the
            # classic layout.  This avoids stale slim pixels in shifted faces.
            foreach ($block in @(@(40,16), @(40,32), @(32,48), @(48,48))) {
                for ($y = $block[1]; $y -lt $block[1] + 16; $y++) {
                    for ($x = $block[0]; $x -lt $block[0] + 16; $x++) {
                        $target.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                    }
                }
            }

            # Right arm base and outer layer.
            foreach ($oy in @(16,32)) {
                Copy-StretchedFace $source $target 44 $oy 3 4 44 $oy 4 4
                Copy-StretchedFace $source $target 47 $oy 3 4 48 $oy 4 4
                Copy-StretchedFace $source $target 40 ($oy+4) 4 12 40 ($oy+4) 4 12
                Copy-StretchedFace $source $target 44 ($oy+4) 3 12 44 ($oy+4) 4 12
                Copy-StretchedFace $source $target 47 ($oy+4) 4 12 48 ($oy+4) 4 12
                Copy-StretchedFace $source $target 51 ($oy+4) 3 12 52 ($oy+4) 4 12
            }

            # Left arm base and outer layer use different X origins.
            foreach ($ox in @(32,48)) {
                $top = $ox + 4
                Copy-StretchedFace $source $target $top 48 3 4 $top 48 4 4
                Copy-StretchedFace $source $target ($top+3) 48 3 4 ($top+4) 48 4 4
                Copy-StretchedFace $source $target $ox 52 4 12 $ox 52 4 12
                Copy-StretchedFace $source $target $top 52 3 12 $top 52 4 12
                Copy-StretchedFace $source $target ($top+3) 52 4 12 ($top+4) 52 4 12
                Copy-StretchedFace $source $target ($top+7) 52 3 12 ($top+8) 52 4 12
            }
            # Normalize partial alpha before saving. The old previewer only
            # behaves reliably with binary transparency.
            for ($y = 0; $y -lt 64; $y++) {
                for ($x = 0; $x -lt 64; $x++) {
                    $pixel = $target.GetPixel($x, $y)
                    if ($pixel.A -eq 0) {
                        $target.SetPixel($x, $y, [System.Drawing.Color]::Transparent)
                    } else {
                        $target.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $pixel.R, $pixel.G, $pixel.B))
                    }
                }
            }
            $target.Save($destinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $target.Dispose() }
    } finally { $source.Dispose() }
}

function Load-LegacyResourceIndex($path) {
    $document = New-Object System.Xml.XmlDocument
    $document.PreserveWhitespace = $true
    if (-not (Test-Path $path)) { return $document }

    # Older test builds and the original launcher can leave this file with an
    # XML declaration that disagrees with its actual bytes. XmlDocument.Load
    # then reports "invalid character in the given encoding". Decode the bytes
    # explicitly and let LoadXml parse Unicode text without trusting that stale
    # declaration.
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -eq 0) { return $document }
    $decoders = @(
        (New-Object System.Text.UTF8Encoding($false, $true)),
        ([System.Text.Encoding]::GetEncoding(
            936,
            [System.Text.EncoderFallback]::ExceptionFallback,
            [System.Text.DecoderFallback]::ExceptionFallback
        ))
    )
    $lastError = $null
    foreach ($decoder in $decoders) {
        try {
            $text = $decoder.GetString($bytes)
            $text = [Text.RegularExpressions.Regex]::Replace(
                $text,
                '^\s*<\?xml[^?]*\?>',
                '',
                [Text.RegularExpressions.RegexOptions]::IgnoreCase
            )
            $document.LoadXml($text)
            return $document
        } catch {
            $lastError = $_.Exception
        }
    }
    throw $lastError
}

function Save-LegacyResourceIndex($document, $indexPath) {
    $settings = New-Object System.Xml.XmlWriterSettings
    $settings.Encoding = [System.Text.Encoding]::GetEncoding(936)
    $settings.Indent = $true
    $settings.NewLineChars = "`r`n"
    $settings.OmitXmlDeclaration = $false
    $temporaryPath = "$indexPath.tmp"
    $writer = [System.Xml.XmlWriter]::Create($temporaryPath, $settings)
    try {
        $writer.WriteStartDocument()
        $document.DocumentElement.WriteTo($writer)
        $writer.WriteEndDocument()
    } finally {
        $writer.Close()
    }
    Move-Item -LiteralPath $temporaryPath -Destination $indexPath -Force
}

function Show-LocalSkinManager {
    $mcboxRoot = Join-Path $env:APPDATA "duowan\mcpcbox"
    $indexPath = Join-Path $mcboxRoot "config\downloaded_res.xml"
    $libraryDir = Join-Path $mcboxRoot "resources\local_skins"
    $libraryRootPrefix = [IO.Path]::GetFullPath($libraryDir).TrimEnd('\') + '\'
    if (-not (Test-Path $indexPath)) {
        [System.Windows.Forms.MessageBox]::Show("尚未找到本地皮肤记录。", "管理本地皮肤") | Out-Null
        return
    }

    $document = Load-LegacyResourceIndex $indexPath
    $nodes = New-Object System.Collections.ArrayList
    foreach ($node in @($document.SelectNodes("/download_res/res[@type='2']"))) {
        $filePath = $node.GetAttribute("file_path")
        if ($filePath) {
            try {
                $fullFilePath = [IO.Path]::GetFullPath($filePath)
                if ($fullFilePath.StartsWith($libraryRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                    [void]$nodes.Add($node)
                }
            } catch {
                # Ignore malformed paths from unrelated legacy entries.
            }
        }
    }
    if ($nodes.Count -eq 0) {
        [System.Windows.Forms.MessageBox]::Show("当前没有由本导入器添加的本地皮肤。", "管理本地皮肤") | Out-Null
        return
    }

    $form = New-Object System.Windows.Forms.Form
    $form.Text = "管理本地皮肤"
    $form.StartPosition = "CenterScreen"
    $form.Size = New-Object System.Drawing.Size(560, 470)
    $form.MinimizeBox = $false
    $form.MaximizeBox = $false

    $tip = New-Object System.Windows.Forms.Label
    $tip.Location = New-Object System.Drawing.Point(15, 15)
    $tip.Size = New-Object System.Drawing.Size(515, 48)
    $tip.Text = "勾选要删除的皮肤卡片。64×64 皮肤通常包含 [1.8+] 和 [1.7] 两张卡片，如需完整删除请同时勾选。"
    $form.Controls.Add($tip)

    $list = New-Object System.Windows.Forms.CheckedListBox
    $list.Location = New-Object System.Drawing.Point(15, 68)
    $list.Size = New-Object System.Drawing.Size(515, 305)
    $list.CheckOnClick = $true
    foreach ($node in $nodes) {
        [void]$list.Items.Add($node.GetAttribute("name"))
    }
    $form.Controls.Add($list)

    $deleteButton = New-Object System.Windows.Forms.Button
    $deleteButton.Text = "删除所选"
    $deleteButton.Location = New-Object System.Drawing.Point(325, 385)
    $deleteButton.Size = New-Object System.Drawing.Size(95, 30)
    $form.Controls.Add($deleteButton)

    $closeButton = New-Object System.Windows.Forms.Button
    $closeButton.Text = "关闭"
    $closeButton.Location = New-Object System.Drawing.Point(435, 385)
    $closeButton.Size = New-Object System.Drawing.Size(95, 30)
    $closeButton.Add_Click({ $form.Close() })
    $form.Controls.Add($closeButton)

    $deleteButton.Add_Click({
        $selected = @($list.CheckedIndices)
        if ($selected.Count -eq 0) {
            [System.Windows.Forms.MessageBox]::Show("请先勾选至少一张皮肤卡片。", "管理本地皮肤") | Out-Null
            return
        }
        $answer = [System.Windows.Forms.MessageBox]::Show(
            "确定删除选中的 $($selected.Count) 张皮肤卡片吗？`n`n删除前会自动备份资源索引。",
            "确认删除",
            [System.Windows.Forms.MessageBoxButtons]::YesNo,
            [System.Windows.Forms.MessageBoxIcon]::Warning
        )
        if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { return }

        try {
            $paths = New-Object System.Collections.ArrayList
            foreach ($index in $selected) {
                $node = $nodes[[int]$index]
                [void]$paths.Add($node.GetAttribute("file_path"))
                [void]$node.ParentNode.RemoveChild($node)
            }
            Copy-Item -LiteralPath $indexPath -Destination "$indexPath.before-delete.bak" -Force
            Save-LegacyResourceIndex $document $indexPath

            foreach ($path in $paths) {
                if (-not $path) { continue }
                $stillUsed = $false
                foreach ($remainingNode in @($document.SelectNodes("/download_res/res"))) {
                    if ($remainingNode.GetAttribute("file_path") -eq $path) {
                        $stillUsed = $true
                        break
                    }
                }
                try {
                    $fullDeletePath = [IO.Path]::GetFullPath($path)
                    $insideLibrary = $fullDeletePath.StartsWith($libraryRootPrefix, [System.StringComparison]::OrdinalIgnoreCase)
                    if (-not $stillUsed -and $insideLibrary -and (Test-Path -LiteralPath $fullDeletePath)) {
                        Remove-Item -LiteralPath $fullDeletePath -Force
                    }
                } catch {
                    # The index entry is removed, but unsafe or malformed paths are never deleted.
                }
            }
            [System.Windows.Forms.MessageBox]::Show("所选皮肤已删除。请重新打开盒子以刷新列表。", "管理本地皮肤") | Out-Null
            $form.Close()
        } catch {
            [System.Windows.Forms.MessageBox]::Show("删除失败：`n$($_.Exception.Message)", "管理本地皮肤") | Out-Null
        }
    })

    [void]$form.ShowDialog()
}

try {
    $action = [System.Windows.Forms.MessageBox]::Show(
        "请选择操作：`n`n是：导入本地皮肤`n否：管理或删除本地皮肤`n取消：退出",
        "多玩盒子本地皮肤工具",
        [System.Windows.Forms.MessageBoxButtons]::YesNoCancel,
        [System.Windows.Forms.MessageBoxIcon]::Question
    )
    if ($action -eq [System.Windows.Forms.DialogResult]::Cancel) { exit }
    if ($action -eq [System.Windows.Forms.DialogResult]::No) {
        Show-LocalSkinManager
        exit
    }

    $skin = Pick-File "请选择本地 Minecraft 皮肤 PNG" "PNG 皮肤文件 (*.png)|*.png"
    if (-not $skin) { exit }

    $image = [System.Drawing.Image]::FromFile($skin)
    $width = $image.Width
    $height = $image.Height
    $image.Dispose()
    if ($width -ne 64 -or ($height -ne 64 -and $height -ne 32)) {
        [System.Windows.Forms.MessageBox]::Show("皮肤必须是 64×64 或 64×32 的 PNG 图片。", "多玩盒子本地皮肤导入") | Out-Null
        exit
    }

    $mcboxRoot = Join-Path $env:APPDATA "duowan\mcpcbox"
    $configDir = Join-Path $mcboxRoot "config"
    $libraryDir = Join-Path $mcboxRoot "resources\local_skins"
    $indexPath = Join-Path $configDir "downloaded_res.xml"
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    New-Item -ItemType Directory -Path $libraryDir -Force | Out-Null

    $hash = (Get-FileHash -Algorithm MD5 -LiteralPath $skin).Hash.ToLowerInvariant()
    $idBytes = [Text.Encoding]::ASCII.GetBytes($hash.Substring(0, 8))
    $id = [Convert]::ToUInt32($hash.Substring(0, 8), 16)
    if ($id -eq 0) { $id = 1 }
    $destination = Join-Path $libraryDir ("{0}.png" -f $id)
    $legacyId = [uint32]($id -bxor 0x01701710)
    if ($legacyId -eq 0 -or $legacyId -eq $id) { $legacyId = [uint32]($id -bxor 0x00010001) }
    $legacyDestination = Join-Path $libraryDir ("{0}.png" -f $legacyId)
    $isSlim = $false
    if ($height -eq 64) {
        $modelChoice = [System.Windows.Forms.MessageBox]::Show(
            "这张皮肤使用哪种手臂模型？`n`n是：纤细/Alex 模型（3 像素手臂）`n否：经典/Steve 模型（4 像素手臂）`n取消：停止导入`n`n注意：多玩盒子使用旧版皮肤渲染模型，兼容转换后的显示效果可能与官方启动器或皮肤站略有差异。",
            "选择 Minecraft 皮肤模型",
            [System.Windows.Forms.MessageBoxButtons]::YesNoCancel,
            [System.Windows.Forms.MessageBoxIcon]::Question
        )
        if ($modelChoice -eq [System.Windows.Forms.DialogResult]::Cancel) { exit }
        $isSlim = ($modelChoice -eq [System.Windows.Forms.DialogResult]::Yes)
    }
    # Re-encode and apply only the narrow compatibility fixes in
    # Normalize-LegacyPng. The original selected file is never modified.
    Normalize-LegacyPng $skin $destination $isSlim
    if ($height -eq 64) {
        Convert-ToLegacy32 $skin $legacyDestination
    }

    $doc = Load-LegacyResourceIndex $indexPath
    if (-not $doc.DocumentElement) {
        [void]$doc.AppendChild($doc.CreateElement("download_res"))
    }
    if (-not $doc.DocumentElement -or $doc.DocumentElement.Name -ne "download_res") {
        throw "downloaded_res.xml 的根节点格式不正确。"
    }

    $baseName = [IO.Path]::GetFileNameWithoutExtension($skin)
    if ($height -eq 64) {
        Add-SkinEntry $doc $id ($baseName + " [1.8+]") $destination
        Add-SkinEntry $doc $legacyId ($baseName + " [1.7]") $legacyDestination
    } else {
        Add-SkinEntry $doc $id ($baseName + " [1.7]") $destination
    }

    $settings = New-Object System.Xml.XmlWriterSettings
    # MCPCBox 2.0 reads resource names through the system ANSI code page even
    # when its own XML writer labels the file as UTF-8.  Writing CP936 keeps
    # Chinese skin names intact on the Chinese Windows builds it targets.
    $settings.Encoding = [System.Text.Encoding]::GetEncoding(936)
    $settings.Indent = $true
    $settings.NewLineChars = "`r`n"
    $settings.OmitXmlDeclaration = $false
    $tempIndexPath = "$indexPath.tmp"
    $writer = [System.Xml.XmlWriter]::Create($tempIndexPath, $settings)
    try {
        # Let XmlWriter create the declaration. Inserting an XmlDeclaration
        # node into a recovered legacy document can fail when invisible
        # whitespace nodes precede its root element.
        $writer.WriteStartDocument()
        $doc.DocumentElement.WriteTo($writer)
        $writer.WriteEndDocument()
    } finally {
        $writer.Close()
    }
    Move-Item -LiteralPath $tempIndexPath -Destination $indexPath -Force

    $modelMessage = if ($isSlim) { "`n已将纤细/Alex 手臂转换为盒子预览兼容格式。" } else { "" }
    $versionMessage = if ($height -eq 64) { "`n已生成 [1.8+] 64×64 和 [1.7] 64×32 两个皮肤条目。" } else { "`n已生成 [1.7] 64×32 皮肤条目。" }
    [System.Windows.Forms.MessageBox]::Show("皮肤已添加到多玩盒子本地皮肤库。$modelMessage$versionMessage`n`n注意：盒子的旧版渲染模型可能与官方启动器或皮肤站显示略有差异。`n`n请完全关闭并重新打开盒子，然后选择与游戏版本对应的皮肤卡片。", "多玩盒子本地皮肤导入") | Out-Null
} catch {
    if ($tempIndexPath -and (Test-Path $tempIndexPath)) {
        Remove-Item -LiteralPath $tempIndexPath -Force
    }
    [System.Windows.Forms.MessageBox]::Show("操作失败：`n$($_.Exception.Message)", "多玩盒子本地皮肤工具") | Out-Null
}
