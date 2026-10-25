# GodFall Content

Content source for GodFall models, clothing, and weapons.

Item folders are under [`GameItems`](GameItems/README.md).

The [Windows startup launcher](launcher/README.md) downloads the complete packaged game from [GitHub release content](GameContent/README.md), then synchronizes additional prepared classic `.pak` files before launch. GitHub Actions publishes the extra-pack list whenever category-folder content changes.

## Setup status

The complete-content installer and native free store are implemented. Source PIE checks passed a56-item free catalog, permanent weapon/clothing claims at zero cost, full-bag rejection, protected paid transactions, automatic equipped-weapon attachment and fresh Continue persistence. In the Vertical Slice, open the pause menu and choose **FREE STORE** beside Back. Characters remain free through character selection. Full-release clean-download verification is the final rollout step.

The [native runtime module and catalog contract](Plugins/GodFallPackRuntime/README.md) also support additional characters/clothing/weapons. Compatible packs belong in `GameItems` with their descriptors; a mesh-only pack does not define a playable item. The original integration passed actual cooked-pack registration/removal in a Windows Development game (each category0→1→0). Disposable verification packs are not published content.
