# Agape48 - la tuta Vindoza ĉeno: agordi, konstrui, disfaldi, tondi, paki.
#
#   pwsh -File tools\build-windows.ps1
#
# La ekvivalento de tools/package-linux.sh, kaj ĝi ekzistas pro la sama kaŭzo
# krom unu: la tondo de la ŝarĝo NE ESTAS DAŬRA. Ĉiu ruligo de windeployqt
# remetas ĉiujn stilojn de Quick Controls kaj la dosierujojn qmltooling, tls kaj
# networkinformation. Tri vicoj re-derivis ĉi tiun sekvencon mane, kaj unu el ili
# celis la stiltondon al la malĝusta vojo, forigis nenion, diris nenion, kaj
# pakis instalilon de 23,6 MB anstataŭ 16,7 MB.
#
# LA VERSIO NE ESTAS ĈI TIE. Ĝi venas el la konstruo kiun ni pakas, ekzakte kiel
# en package-linux.sh: la agorda paŝo skribis CMakeCache.txt el project(...
# VERSION x.y.z), do ĝi ne povas malkonsenti kun la duumaĵo.

param(
    # Du kernoj restas liberaj defaŭlte. Ĉi tiu komputilo estas uzata dum ĝi
    # konstruas, kaj plena ŝarĝo faras la maŝinon neuzebla.
    [int]$Jobs = [Math]::Max(1, [int]$env:NUMBER_OF_PROCESSORS - 2)
)

$ErrorActionPreference = "Stop"

$env:Path = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;" + $env:Path
$r    = (Resolve-Path "$PSScriptRoot\..").Path
$bld  = "$r\build-mingw"
$pay  = "$r\installer\payload"
$iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

function Weigh($label) {
    $f = Get-ChildItem $pay -Recurse -File
    "  {0,-22} {1,5} files  {2,7:N1} MB" -f $label, $f.Count, (($f | Measure-Object -Sum Length).Sum / 1MB)
}

"=== 1. la ikonoj  (ANTAŬ la konstruo: assets/icon.png estas enmetita en la duumaĵon) ==="
python "$r\tools\make-icon.py" | Select-Object -Last 1
if (-not (Test-Path "$r\installer\agape48.ico")) { throw "make-icon.py ne skribis agape48.ico" }

"=== 2. agordi kaj konstrui  (LTO OFF: ĝi mortas en GCC-ICE ĉe la fina ligo) ==="
cmake -S $r -B $bld -G Ninja -DAGAPE48_LTO=OFF -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 `
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ | Select-Object -Last 2
cmake --build $bld -j $Jobs | Select-Object -Last 2
"  agape48.exe {0:N0} bajtoj  (-j {1})" -f (Get-Item "$bld\agape48.exe").Length, $Jobs

$version = (Select-String -Path "$bld\CMakeCache.txt" -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.+)$' `
            | Select-Object -First 1).Matches.Groups[1].Value
if (-not $version) {
    $version = (Select-String -Path "$r\CMakeLists.txt" -Pattern '^\s*VERSION\s+([0-9][0-9.]*)\s*$' `
                | Select-Object -First 1).Matches.Groups[1].Value
}
if (-not $version) { throw "ne povas legi la version el CMakeCache.txt nek el CMakeLists.txt" }
"  versio $version"

"=== 3. disfaldi  (--compiler-runtime NE estas malnepra por MinGW) ==="
Remove-Item -LiteralPath $pay -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $pay | Out-Null
Copy-Item "$bld\agape48.exe" $pay
windeployqt --release --compiler-runtime --qmldir "$r\qml" "$pay\agape48.exe" | Out-Null
Weigh "post windeployqt"

"=== 4. tondi ==="
# main.cpp fiksas QQuickStyle al Basic kaj objdump montras ke Qt6QuickControls2.dll
# importas nenian stilrealigon, do la aliaj neniam estas ŝargataj. NOTU LA VOJON:
# windeployqt metas ĉi tiujn sub payload\qml\, ne sub payload\.
foreach ($s in @("FluentWinUI3", "Fusion", "Imagine", "Material", "Universal", "Windows")) {
    $d = "$pay\qml\QtQuick\Controls\$s"
    if (Test-Path $d) { Remove-Item -LiteralPath $d -Recurse -Force }
}
foreach ($d in @("FluentWinUI3StyleImpl", "Fusion", "FusionStyleImpl", "Imagine", "ImagineStyleImpl",
                 "Material", "MaterialStyleImpl", "Universal", "UniversalStyleImpl", "WindowsStyleImpl")) {
    $f = "$pay\Qt6QuickControls2$d.dll"
    if (Test-Path $f) { Remove-Item -LiteralPath $f -Force }
}
# qt_import_plugins(EXCLUDE_BY_TYPE ...) validas nur por STATIKA konstruo.
foreach ($s in @("qmltooling", "tls", "networkinformation")) {
    $d = "$pay\$s"
    if (Test-Path $d) { Remove-Item -LiteralPath $d -Recurse -Force }
}
# opengl32sw estas la programa rastrumilo al kiu Qt retiriĝas kiam ne ekzistas
# funkcianta OpenGL-pelilo - ekstere de 2026aug31, kaj tio estas revizienda
# antaŭ ol ĉi tio atingos aliajn homojn.
foreach ($n in @("opengl32sw.dll", "D3Dcompiler_47.dll")) {
    if (Test-Path "$pay\$n") { Remove-Item -LiteralPath "$pay\$n" -Force }
}
if (Test-Path "$pay\translations") { Remove-Item -LiteralPath "$pay\translations" -Recurse -Force }
Weigh "post tondo"

$kept = (Get-ChildItem "$pay\qml\QtQuick\Controls" -Directory | Select-Object -ExpandProperty Name) -join " "
if ($kept -ne "Basic impl") { throw "la stiltondo maltrafis: restis '$kept', atendite 'Basic impl'" }
"  stiloj konservitaj: $kept"

"=== 5. paki ==="
& $iscc "/DAppVersion=$version" "$r\installer\agape48.iss" | Select-Object -Last 2
$setup = Get-Item "$r\installer\Agape48-$version-windows-x64-setup.exe"
"  {0}  {1:N0} bajtoj = {2:N1} MB" -f $setup.Name, $setup.Length, ($setup.Length / 1MB)
"  sha256 {0}" -f (Get-FileHash $setup.FullName -Algorithm SHA256).Hash.ToLower()
