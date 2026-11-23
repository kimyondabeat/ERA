#!/bin/bash
# Builds ERA_v1.0_Mac.pkg: AU -> /Library/Audio/Plug-Ins/Components, VST3 -> /Library/Audio/Plug-Ins/VST3, app -> /Applications
set -e
A="$1"
W=$(mktemp -d)
mkdir -p "$W/au" "$W/vst3" "$W/app"
cp -R "$A/AU/ERA.component" "$W/au/"
cp -R "$A/VST3/ERA.vst3" "$W/vst3/"
cp -R "$A/Standalone/ERA.app" "$W/app/"
pkgbuild --root "$W/au"   --install-location "/Library/Audio/Plug-Ins/Components" --identifier com.kimyondabeat.era.au   --version 1.0.0 "$W/au.pkg"
pkgbuild --root "$W/vst3" --install-location "/Library/Audio/Plug-Ins/VST3"       --identifier com.kimyondabeat.era.vst3 --version 1.0.0 "$W/vst3.pkg"
pkgbuild --root "$W/app"  --install-location "/Applications"                      --identifier com.kimyondabeat.era.app  --version 1.0.0 "$W/app.pkg"
cat > "$W/dist.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>ERA - Raboday Drums v1.0</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>
    <line choice="au"/><line choice="vst3"/><line choice="app"/>
  </choices-outline>
  <choice id="au"   title="ERA Audio Unit (Logic, GarageBand...)"><pkg-ref id="com.kimyondabeat.era.au"/></choice>
  <choice id="vst3" title="ERA VST3 plugin"><pkg-ref id="com.kimyondabeat.era.vst3"/></choice>
  <choice id="app"  title="ERA app"><pkg-ref id="com.kimyondabeat.era.app"/></choice>
  <pkg-ref id="com.kimyondabeat.era.au">au.pkg</pkg-ref>
  <pkg-ref id="com.kimyondabeat.era.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="com.kimyondabeat.era.app">app.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$W/dist.xml" --package-path "$W" ERA_v1.0_Mac.pkg
echo "Built ERA_v1.0_Mac.pkg"
