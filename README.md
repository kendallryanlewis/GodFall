# GodFall Content

Content source for GodFall models, clothing, and weapons.

Item folders are under [`GameItems`](GameItems/README.md).

The [Windows startup launcher](launcher/README.md) downloads the complete packaged game from [GitHub release content](GameContent/README.md), then synchronizes additional prepared classic `.pak` files before launch. GitHub Actions publishes the extra-pack list whenever category-folder content changes.

## Setup status

Download [GodFall-Launcher.zip](https://github.com/kendallryanlewis/GodFall/releases/download/godfall-content-20261024-free-store/GodFall-Launcher.zip), follow its HOW-TO-PLAY instructions, and open `Play-GodFall.cmd`. The first launch downloads the complete game from GitHub; later launches check for updates. A clean install downloaded and verified all 52 packaged files and launched the Windows Development build with isolated player data.

In the Vertical Slice, open the pause menu and choose **FREE STORE** beside Back. Source PIE checks passed the 56-item free catalog, permanent weapon/clothing claims at zero cost, full-bag rejection, protected paid transactions, automatic equipped-weapon attachment and fresh Continue persistence. Characters remain free through character selection. This release is a Development preview.

The [native runtime module and catalog contract](Plugins/GodFallPackRuntime/README.md) also support additional characters/clothing/weapons. Compatible packs belong in `GameItems` with their descriptors; a mesh-only pack does not define a playable item. The original integration passed actual cooked-pack registration/removal in a Windows Development game (each category0→1→0). Disposable verification packs are not published content.
