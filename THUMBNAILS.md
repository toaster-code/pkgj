# Image Panel System in PKGj

## Overview
This document explains how the game image panel works after the image-loading code was unified around `ImageFetcher`.

The system now supports two image sources through a single implementation:
- a custom local/remote image source configured by the user
- the legacy PS Store cover URL used as a fallback

The goal is to keep the image path simple while preserving the custom thumbnail feature.

## Files involved
- [src/imagefetcher.hpp](src/imagefetcher.hpp): `ImageFetcher` declaration.
- [src/imagefetcher.cpp](src/imagefetcher.cpp): background loading, caching, download limits, and fallback logic.
- [src/gameview.hpp](src/gameview.hpp): `GameView` member that owns the fetcher.
- [src/gameview.cpp](src/gameview.cpp): image panel rendering and `ImageFetcher` construction.
- [src/config.hpp](src/config.hpp): `thumbnail_url`, `thumbnail_folder`, and `thumbnail_size` settings.
- [src/config.cpp](src/config.cpp): config parsing and persistence.

## Resolution order
When the details view needs an image, `ImageFetcher` resolves it in this order:

1. Load `{folder}/{titleid}.jpg` from local storage.
2. If the file does not exist and `thumbnail_url` is configured, download `{thumbnail_url}/{titleid}.jpg`.
3. If `thumbnail_url` is empty, fall back to the legacy PS Store cover URL derived from the selected game.
4. Cache any successfully downloaded image to the local folder for the next open.

This means custom images still work, but the old cover-fetching behavior remains available without a second fetcher class.

## Configuration
Relevant `config.txt` keys:

```txt
thumbnail_folder ux0:pkgj/cover
thumbnail_url https://example.com/thumbs
thumbnail_size 2
```

- `thumbnail_folder`: local cache folder. If empty, PKGj uses `ux0:pkgj/cover`.
- `thumbnail_url`: optional custom base URL. If empty, PKGj falls back to the PS Store cover URL.
- `thumbnail_size`: controls the panel size in `GameView`.

You can also manually copy JPEG files into the configured folder using the Title ID as the file name.

## ImageFetcher structure
`ImageFetcher` is defined in [src/imagefetcher.hpp](src/imagefetcher.hpp) and implemented in [src/imagefetcher.cpp](src/imagefetcher.cpp).

Public interface:
- `ImageFetcher(const Config* config, DbItem* item)`
- `~ImageFetcher()`
- `vita2d_texture* get_texture()`

Key responsibilities:
- build the correct local cache path
- build the effective URL for either custom images or PS Store fallback
- load and decode JPEGs in a background thread
- abort safely when the details view closes
- cache downloaded files atomically

Important implementation details:
- maximum download size: 100 KB
- download timeout: 8 seconds
- format: JPEG only
- file writes use a temporary file plus rename to avoid partial cache corruption
- texture destruction waits for GPU rendering to finish before freeing the texture

## Integration in GameView
`GameView` owns a single `ImageFetcher` instance and renders its texture in the top-right image panel.

Current integration points:
- fetcher member in [src/gameview.hpp](src/gameview.hpp)
- fetcher construction in [src/gameview.cpp](src/gameview.cpp)
- texture consumption during rendering in [src/gameview.cpp](src/gameview.cpp)

The panel stays pinned to the top-right corner and is drawn outside the normal widget navigation flow so it cannot receive focus from the D-pad.

## Panel behavior
- If a texture is available, the image is scaled to fit while preserving aspect ratio.
- If no texture is available, the panel shows a centered placeholder message.
- The panel uses a fixed overlay draw path so it is not affected by scroll position.

## Practical usage
### Custom image pack
Host or copy files named like:

```txt
PCSG00001.jpg
PCSB01234.jpg
NPUB30000.jpg
```

Then set:

```txt
thumbnail_folder ux0:pkgj/cover
thumbnail_url https://myserver.com/pkgj-images
```

### Legacy cover behavior
If you leave `thumbnail_url` empty, PKGj uses the old PS Store cover lookup automatically.

## Summary
- `ThumbnailFetcher` was removed.
- `ImageFetcher` now handles both custom images and legacy store covers.
- The UI still uses the same image panel and size settings.
- The implementation keeps the safer shutdown and caching behavior needed for rapid open/close cycles.
