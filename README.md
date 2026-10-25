# GodFall Content

Content source for GodFall characters, armor, and weapons.

Item folders are under [`GameItems`](GameItems/README.md).

The [Windows startup launcher](launcher/README.md) downloads the complete packaged game from [GitHub release content](GameContent/README.md), then synchronizes additional prepared classic `.pak` files before launch. GitHub Actions publishes the extra-pack list whenever category-folder content changes.

## Setup status

Download [GodFall-Launcher.zip](https://github.com/kendallryanlewis/GodFall/releases/download/godfall-content-20261025-github-items/GodFall-Launcher.zip), follow its HOW-TO-PLAY instructions, and open `Play-GodFall.cmd`. The first launch downloads the complete game from GitHub; later launches check for updates. Player data stays outside the version folders.

In the Vertical Slice, walk up to the **Quartermaster** beside the practice-area starting path and press **E** (gamepad X / Square) to open the **Free Store**. Claim armor and weapons at no cost, then use Back or Escape to return to gameplay. The pause menu no longer has a store button. Characters remain free through character selection. This release is a Development preview.

Usable GitHub armor and weapons supply generic loot and free-store offers. If the catalog is empty or unavailable, local armor and weapons are available instead. The editor plugin also synchronizes packs when GodFall_Clean opens; restart the editor after changing published packs. Downloads stay in a separate Saved cache, and existing source items and player saves are preserved.

The [native runtime module and catalog contract](Plugins/GodFallPackRuntime/README.md) also support additional characters, armor, and weapons. Compatible packs belong in `GameItems` with their descriptors; a mesh-only pack does not define a playable item. The original integration passed actual cooked-pack registration/removal in a Windows Development game (each category0→1→0). Disposable verification packs are not published content.
