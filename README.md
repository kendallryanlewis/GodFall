# GodFall Content

Content source for GodFall models, clothing, and weapons.

Item folders are under [`GameItems`](GameItems/README.md).

The [Windows startup launcher](launcher/README.md) synchronizes prepared classic `.pak` files before starting a packaged game. GitHub Actions publishes the exact pack list whenever category-folder content changes.

## Setup status

Repository folders, descriptor/manifest generation, and the pre-launch downloader are provided. The [native runtime module and catalog contract](Plugins/GodFallPackRuntime/README.md) are under integration with GodFall_Clean. Actual Unreal mounting and playable-item registration still require Blueprint integration and packaged-game verification. Uploading a mesh-only pack does not automatically make it an equipment item or playable character.
