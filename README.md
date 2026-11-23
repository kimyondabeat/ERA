# ERA – Raboday Drums v1.0

Raboday drum plugin by kimyondabeat. VST3 + app (Windows, Mac), AU (Mac).

- **Windows:** GitHub Actions → "Build ERA for Windows" → release `v1.0` → `ERA_v1.0_Setup.exe`
- **Mac:** GitHub Actions → "Build ERA for Mac" → release `v1.0` → `ERA_v1.0_Mac.pkg`

## Sounds
Every file in `Kit/` is built into the plugin. Names: `<category>__<NN>__<Name>.flac`
(categories: `kick snare rim percs vox fx clap tom`). The current kit is a synthesized
placeholder (`tools/make_placeholder_kit.py`); replace the files with your own pack and push.

## Build on your computer
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
