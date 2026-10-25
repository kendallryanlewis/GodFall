# Complete game content

Get the [standalone launcher ZIP](https://github.com/kendallryanlewis/GodFall/releases/download/godfall-content-20261025-github-items/GodFall-Launcher.zip) from the Windows Development preview. Extract its launcher files together and follow HOW-TO-PLAY.txt.

The full-content installer reads `manifest.json` here, then downloads the exact Windows build from its immutable GitHub release. The generated manifest names every installed file and its SHA-256 hash. Large archives are split into768MiB release assets and verified before extraction.

Keep `Play-GodFall.cmd`, `Start-GodFallRelease.ps1`, and `Start-GodFall.ps1` together. No game content needs to be bundled with these launcher files. `Play-GodFall.cmd` installs into `%LOCALAPPDATA%\GodFallGitHub`, checks for updates before launch, and keeps player data separately under `PlayerData`.

A complete new version is staged and verified before the active version pointer changes. Removed files are absent from the new active version. Earlier versions remain outside the active game folder for rollback. A failed connection can use a previously verified installation; a first installation requires a successful download. The original Unreal source project and other installed builds are not installation targets.

Additional prepared content packs use `GameItems/Models`, `Armor`, and `Weapons` and are synchronized after the base release. The legacy `Clothing` folder remains compatible with armor packs. If the optional catalog is empty or unavailable, the verified base game supplies local armor and weapons for loot and the free store. Arbitrary source meshes are not cooked game items. Author a compatible full build or item pack using the runtime contract.

To prepare a full-content release from a packaged Windows build:

```powershell
python scripts/build_release.py --source C:\Builds\GodFall\Windows --output C:\Builds\GodFallRelease --tag godfall-content-VERSION --revision FULL_GIT_COMMIT_SHA
```

Publish the generated `.partNNN` files and `manifest.json` as assets of the matching GitHub release. After checking the release downloads and build, publish its generated manifest to `GameContent/manifest.json`. Do not hand-edit hashes or reuse a release tag for different bytes. Game code or gameplay content changes need a newly cooked compatible build; folder edits alone do not recook Unreal content.
