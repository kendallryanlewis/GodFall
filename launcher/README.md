# GodFall startup content sync

This first version is a **Windows launcher**, not an in-game downloader. It synchronizes classic cooked `.pak` files before starting a packaged GodFall build. Launch through this script whenever you want an update check; starting the executable directly skips the check.

The folder setup is published. The downloader can be tested separately from Unreal. **Playable-item registration, actual Unreal pack mounting, and saved-item recovery still require integration and a packaged-game test.** Do not treat a successful sync as proof that a new mesh is playable.

## Publishing packs

1. Build compatible content for the same GodFall project, UE 5.8 version/build, Win64 platform, and base release. Use classic Pak packaging with IoStore disabled for this first workflow. Include cooked assets and their required dependencies; raw FBX, GLB, or editor `.uasset` files alone are not sufficient.
2. Upload packs into `GameItems/Models/`, `GameItems/Clothing/`, or `GameItems/Weapons/` on `main`. Subfolders are allowed. Use letters, digits, hyphens, and underscores in names. Each pack must be nonempty and at most 95 MiB; Git LFS pointers and `.utoc`/`.ucas` containers are rejected. Larger release downloads are a future extension.
3. Wait for **Publish GodFall item list** in GitHub Actions to succeed. It regenerates `GameItems/manifest.json` with sizes, SHA-256 hashes, and a pinned source commit. Repository policies must allow that workflow to write the manifest to `main`. If a policy prevents its push, publish the generated manifest through the repository's permitted review process.
4. Delete a pack from its category folder and wait for that workflow to succeed to remove it from the next synchronized set. Do not edit the generated manifest by hand.

Individual packs are not a complete item-authoring contract yet. GodFall's existing armor catalog, weapon registry, and playable-character definitions must be connected to the downloaded content. Two packs replacing the same existing catalog would conflict, so do not publish catalog-replacement packs as a substitute for that integration.

## Launching a packaged game

Keep this launcher outside its managed cache. The example paths below are placeholders; use your actual packaged build, **not the live Unreal project**.

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

## Remaining game integration

The next milestone must connect downloaded definition assets to the actual armor, weapon, and character authorities, exercise an added and removed compatible item in a packaged build, verify missing equipped-item recovery, and check protected inventory, equipment, input, animation, and save persistence. No production Blueprint or project asset has been changed by this repository setup.
