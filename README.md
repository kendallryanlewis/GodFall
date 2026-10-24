# GodFall Content

Content source for GodFall models, clothing, and weapons.

Item folders are under [`GameItems`](GameItems/README.md).

The [Windows startup launcher](launcher/README.md) synchronizes prepared classic `.pak` files before starting a packaged game. GitHub Actions publishes the exact pack list whenever category-folder content changes.

## Setup status

Repository folders, descriptor/manifest generation, and the pre-launch downloader are provided. The [native runtime module and catalog contract](Plugins/GodFallPackRuntime/README.md) are integrated with GodFall_Clean's saved Blueprint routes. Private editor checks passed additional-character selection/restoration, missing-character continuation checks, clothing loot/equipment, permanent weapon acquisition/equipment/persistence, existing transaction costs and capacity rules. A rebuilt Windows game and actual cooked-pack add/remove verification are in progress. Uploading a mesh-only pack does not automatically make it an equipment item or playable character.
