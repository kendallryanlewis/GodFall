# GodFall Content

Content source for GodFall models, clothing, and weapons.

Item folders are under [`GameItems`](GameItems/README.md).

The [Windows startup launcher](launcher/README.md) synchronizes prepared classic `.pak` files before starting a packaged game. GitHub Actions publishes the exact pack list whenever category-folder content changes.

## Setup status

Repository folders, descriptor/manifest generation, and the pre-launch downloader are provided. The [native runtime module and catalog contract](Plugins/GodFallPackRuntime/README.md) are integrated with GodFall_Clean's saved Blueprint routes. Private editor checks passed additional-character selection/restoration, missing-character continuation checks, clothing loot/equipment, permanent weapon acquisition/equipment/persistence, existing transaction costs and capacity rules. A rebuilt Windows Development game passed real cooked-pack synchronization and fresh-process registration/removal: character, clothing and weapon counts each changed 0 → 1 → 0. Test fixtures reuse existing art and are not published content. Upload your compatible packs and descriptors, then launch through `Play-GodFall.cmd`. Uploading a mesh-only pack does not automatically make it an equipment item or playable character.
