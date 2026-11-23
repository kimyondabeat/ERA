# ERA – Raboday Drums v1.0

Raboday drum plugin by kimyondabeat. VST3 + app (Windows, Mac), AU (Mac).

- **Windows:** GitHub Actions → "Build ERA for Windows" → release `v1.0` → `ERA_v1.0_Setup.exe`
- **Mac:** GitHub Actions → "Build ERA for Mac" → release `v1.0` → `ERA_v1.0_Mac.pkg`

## Sounds
Every file in `Kit/` is built into the plugin. Names: `<category>__<NN>__<Name>.flac`
(categories: `kick snare rim percs vox fx clap tom`). The first file of each category is the pad's
default sound. Add, remove or rename files, push, and the plugin is rebuilt with them.

## Build on your computer
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
