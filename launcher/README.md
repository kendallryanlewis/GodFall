# GodFall startup content sync

`Play-GodFall.cmd` is the Windows entry point. Keep it beside `Start-GodFallRelease.ps1` and `Start-GodFall.ps1`. The first launch downloads the complete game from its published GitHub release; later launches check for a new complete version and additional item packs before starting. Game content is installed under `%LOCALAPPDATA%\GodFallGitHub\versions`; player data is separate under `PlayerData`. See the [full-content contract](../GameContent/README.md).

`Start-GodFall.ps1` remains the additional-pack synchronizer for an already packaged game. Starting the game's executable directly skips update checks.

The folders and downloader are published. Private editor fixtures passed new-character selection/restoration, clothing loot/equipment, permanent weapon acquisition, existing transaction/capacity rules and equipped-weapon save restoration. A rebuilt Windows Development game passed actual cooked-pack synchronization and fresh-process registration/removal: each external content category changed 0 → 1 → 0, with no registry issues and isolated test saves.

## Publishing packs

1. Build compatible content for the same GodFall project, UE 5.8 version/build, Win64 platform, and base release. Use classic Pak packaging with IoStore disabled for this first workflow. Include cooked assets and their required dependencies; raw FBX, GLB, or editor `.uasset` files alone are not sufficient.
2. Upload packs and adjacent same-basename `.pack.json` descriptors into `GameItems/Models/`, `GameItems/Clothing/`, or `GameItems/Weapons/` on `main`. Each descriptor names the cooked catalog described in the [runtime contract](../Plugins/GodFallPackRuntime/README.md). Subfolders are allowed. Use letters, digits, hyphens, and underscores in names. Each pack must be nonempty and at most 95 MiB; Git LFS pointers and `.utoc`/`.ucas` containers are rejected. Larger release downloads are a future extension.
3. Wait for **Publish GodFall item list** in GitHub Actions to succeed. It regenerates `GameItems/manifest.json` with sizes, SHA-256 hashes, and a pinned source commit. Repository policies must allow that workflow to write the manifest to `main`. If a policy prevents its push, publish the generated manifest through the repository's permitted review process.
4. Delete a pack and its descriptor from the category folder, then wait for that workflow to succeed to remove it from the next synchronized set. Do not edit the generated manifest by hand.

The runtime catalog declares character profiles, clothing/weapon definitions and optional authored acquisition rows. Matching base-game Blueprint routes are saved and passed focused private PIE checks. Packs must provide their own namespaced definitions and catalogs while retaining existing base assets.

## Launching a packaged game

Keep this launcher outside its managed cache. The example paths below are placeholders; use your actual packaged build, **not the live Unreal project**.

For a clean installation, use the three-file launcher described above. No game executable or assets need to be supplied beside it. To manage only extra packs for a separately installed packaged build, use the PowerShell command below with that build's paths.

```powershell
powershell.exe -NoProfile -File .\Start-GodFall.ps1 `
  -GameRoot 'C:\Games\GodFall\GodFall_Clean' `
  -GameExe 'C:\Games\GodFall\GodFall_Clean.exe'
```

`GameRoot` is the packaged project directory that already contains `Content\Paks`. `GameExe` is that build's bootstrap executable or its `Binaries\Win64\GodFall_Clean-Win64-Shipping.exe`. The source project is rejected. For a sync without launching, provide `-SyncOnly` and omit `-GameExe`.

The game must be closed. The launcher needs write access to the packaged build's Content directory and a reasonably short install path for Windows PowerShell 5.1. It reads public GitHub content without a token; private repositories are not supported by this version. Follow your Windows execution policy for scripts rather than changing machine-wide policy.

## Sync behavior

- The only active pack directory it manages is `Content\Paks\GodFallGitHub`. Unreal's installed 5.8 PakFile source includes `Content\Paks` in startup discovery and searches recursively, including in Shipping builds. Engine command-line `-pakdir` support is not used because it is excluded in Shipping builds.
- All files are downloaded and checked before replacing the active set. Unchanged verified packs are reused. Old managed packs disappear from the active directory only after the entire replacement set is ready.
- `Content\GodFallGitHubSync` holds the lock, staging, and previous set outside Unreal's Pak search directory. Directory ownership records prevent adopting or deleting unrelated folders. Unknown files and linked paths stop synchronization.
- Interrupted promotion can recover the previous set on the next run. A failed download or invalid manifest keeps the last successful, fully verified set. If there is no valid previous set, the launcher stops rather than launching with a partial update.
- A successfully published empty manifest deliberately clears the **managed pack set only**. Built-in packs, source assets, and save files are outside its write scope.

Packs use deterministic `_P.pak` names so they participate in Unreal's patch ordering. Authoring and testing must ensure they contain only the intended content and do not accidentally override unrelated base-game packages.

## Verification status

The live GodFall_Clean project and rebuilt classic-Pak Windows Development game contain the matching registry, selection, clothing-query and acquisition routes. Later compatible content updates use the launcher. Verification used UE 5.8.2-56702186+++UE5+Release-5.8, Win64: 25 launcher regression checks and a 54-check real archive/add/remove test passed. Editor PIE exercised gameplay separately; the packaged check exercised fresh startup registration/removal. Test fixtures are not release content. A Shipping build and physical gamepad were not exercised. Source assets and production saves are preserved.

The full-content installer passed13 separate filesystem/HTTP regressions for clean install, unchanged reuse, active-version additions/removals, save preservation, invalid paths, corrupt downloads/cache and failure recovery. Both base content and optional packs must be ready before the active version pointer changes. Full-release public download/launch verification is pending rollout. In-game free claims are available through the Vertical Slice pause menu; ordinary paid offers retain their authored prices.
