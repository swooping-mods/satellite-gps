# Satellite GPS

Straight-line driven GPS with purchased autonomous car modules and map commands.

## Install

Install [Moon Sweepers Mod Loader](https://github.com/swooping-mods/moon-sweepers-mod-loader) first.

Download [satellite-gps-0.1.0.msmod](content/satellite-gps-0.1.0.msmod), then drag and drop the `.msmod` file onto the [Moon Sweepers Mod Loader](https://github.com/swooping-mods/moon-sweepers-mod-loader) window. You can also use **Add mods…** (older loader builds: **Install mod package**).

Select the mod and its required frameworks, click **Prepare files**, then **Launch**.

Required mods: [vehicle-control-framework](https://github.com/swooping-mods/vehicle-control-framework), [map-control-framework](https://github.com/swooping-mods/map-control-framework), [shop-framework](https://github.com/swooping-mods/shop-framework), [hints-framework](https://github.com/swooping-mods/hints-framework).

## Controls

- Driven GPS is free: enable native cruise and press K to follow your heading in a straight line around the moon.
- Board an owned car, then buy its $750 Satellite Autonomous Module in Modded Shop.
- Use the map's top Vehicles / GPS row to select a car, capture heading or A/B line, follow it, drive to a right-clicked point, or stop.
- Autonomous driving is host-only and stops for obstacles; it does not plan detours or U-turns.

## Compatibility

Version 0.1.0. Targets the pinned Steam Windows x64 IL2CPP build of Moon Sweepers (Steam app 5335510). On Linux, the game runs through Proton. Experimental: back up saves before using save-affecting mods.

## Build

Run `python3 build.py --check` to verify source hashes, then `python3 build.py` to write a build under `.local/build/`. Requires installed Clang, lld-link and llvm-dlltool.

## Credits

See [CREDITS.txt](CREDITS.txt) and [third-party notices](vendored/THIRD_PARTY_NOTICES.md).
