# Thumbnails System in PKGj

## Overview
This document explains how the thumbnail (game screenshot) system works in PKGj, and details the code changes made to improve robustness and user experience. It is intended for developers and contributors who wish to understand or modify the thumbnail feature in the source code.

---

## What are thumbnails?
Thumbnails are small JPEG images shown next to each game in the PKGj UI, providing a visual preview for the user.

---

## Files Involved
The thumbnail system is implemented across several files:
- **[src/thumbnailfetcher.hpp](src/thumbnailfetcher.hpp)**: Class declaration for `ThumbnailFetcher`.
- **[src/thumbnailfetcher.cpp](src/thumbnailfetcher.cpp)**: Implementation of `ThumbnailFetcher`, including the background fetching logic.
- **[src/gameview.hpp](src/gameview.hpp)**: Declaration of `_thumbnail_fetcher` member in `GameView` class.
- **[src/gameview.cpp](src/gameview.cpp)**: Initialization of `ThumbnailFetcher` in `GameView` constructor and rendering in `render()` method.
- **[src/config.hpp](src/config.hpp)**: `Config` struct includes `thumbnail_url` and `thumbnail_folder` fields.
- **[src/config.cpp](src/config.cpp)**: Loading and saving of thumbnail settings from/to `config.txt`.

---

## How the thumbnail system works

### 1. Thumbnail location and naming
- Thumbnails are stored as JPEG files in the folder specified by the `thumbnail_folder` setting in `config.txt`.
- The default/recommended path is:
  - `ux0:pkgj/thumbnails` (on PS Vita)
- Each file is named after the game's Title ID, e.g. `PCSG00001.jpg`.

### 2. Fetching logic (resolution order)
When a thumbnail is needed, the following steps occur:
1. **Try to load from local storage**: `{thumbnail_folder}/{titleid}.jpg`.
2. **If not found and a URL is configured** (`thumbnail_url`), attempt to download from `{thumbnail_url}/{titleid}.jpg`.
3. **If download succeeds**, save the file locally for future use.

### 3. Configuration
To enable thumbnails, add the following lines to your `config.txt`:
```
thumbnail_folder ux0:pkgj/thumbnails
thumbnail_url https://example.com/thumbs
```
- If you do not want network fetching, leave `thumbnail_url` empty.
- You can manually copy JPEG files to the thumbnail folder if desired.
- The settings are loaded in `pkgi_load_config()` in [src/config.cpp](src/config.cpp) and saved in `pkgi_save_config()`.

### 4. Class Structure: ThumbnailFetcher
The `ThumbnailFetcher` class is defined in [src/thumbnailfetcher.hpp](src/thumbnailfetcher.hpp) and implemented in [src/thumbnailfetcher.cpp](src/thumbnailfetcher.cpp).

#### Public Interface
- **Constructor**: `ThumbnailFetcher(const std::string& titleid, const std::string& folder, const std::string& base_url)`
  - Initializes the fetcher with the game's Title ID, local folder path, and base URL for downloads.
  - Starts a background thread to perform the fetching.
- **Destructor**: `~ThumbnailFetcher()`
  - Sets the abort flag, aborts any ongoing HTTP request, and joins the thread to ensure clean shutdown.
- **get_texture()**: `vita2d_texture* get_texture()`
  - Returns the loaded texture pointer if available, or `nullptr` otherwise.
  - Thread-safe via mutex.

#### Private Members
- `_mutex`: `Mutex` for thread-safe access to shared data.
- `_path`: `std::string` - Full local file path (e.g., `ux0:pkgj/thumbnails/PCSG00001.jpg`).
- `_url`: `std::string` - Full download URL (e.g., `https://example.com/thumbs/PCSG00001.jpg`), or empty if disabled.
- `_abort`: `bool` - Flag to signal cancellation.
- `_http`: `std::unique_ptr<Http>` - HTTP client for downloads.
- `_texture`: `vita2d_texture*` - Pointer to the loaded Vita2D texture.
- `_thread`: `Thread` - Background thread executing `do_request()`.

#### Key Method: do_request()
- Runs in a background thread.
- First, attempts to load the JPEG from local storage using `pkgi_file_exists()` and `vita2d_load_JPEG_file()`.
- If not found and URL is set, initiates HTTP download with size limit (100 KB).
- Decodes the downloaded data into a Vita2D texture using `vita2d_load_JPEG_buffer()` and immediately displays it in the UI for the selected game.
- Saves the downloaded file locally for caching, so future requests for the same game use the local file instead of the network.
- Includes timeout check (8 seconds) to prevent hangs.
- Handles exceptions and cleans up resources.

### 5. Integration in GameView
- In [src/gameview.hpp](src/gameview.hpp), `GameView` has a member: `std::unique_ptr<ThumbnailFetcher> _thumbnail_fetcher;`
- In the constructor ([src/gameview.cpp](src/gameview.cpp)), it creates the fetcher:
  ```cpp
  const std::string thumb_folder = config->thumbnail_folder.empty()
          ? "ux0:pkgj/thumbnails"
          : config->thumbnail_folder;
  _thumbnail_fetcher = std::make_unique<ThumbnailFetcher>(
          item->titleid, thumb_folder, config->thumbnail_url);
  ```
- In the `render()` method, the thumbnail is displayed in a **fixed framed panel** pinned to the top-right corner of the game info window. See section [7. Thumbnail panel rendering](#7-thumbnail-panel-rendering-gameview) for full details.

---

### 7. Thumbnail panel rendering (GameView)

The thumbnail is shown in a 200×170 px framed panel pinned to the **top-right corner** of the game info window, regardless of scroll position.

#### Why `GetForegroundDrawList()` + compile-time anchors

Earlier attempts used `ImGui::BeginChild()` and `GetWindowDrawList()`. Both suffered from the same root problem: ImGui's gamepad navigation can scroll the window content even when `NoScrollWithMouse` is set, which caused the panel to drift upward and cover the window title bar.

The final approach uses two key decisions:

1. **`ImGui::GetForegroundDrawList()`** — a global overlay draw list that has no clip rectangle and is completely outside ImGui's widget/navigation system. Primitives drawn here are always at their exact screen coordinates and can never be focused or scrolled by the D-pad.

2. **Compile-time anchors only** — the panel position is computed exclusively from:
   - `ImGui::GetWindowPos()` — the window's screen position (constant, since the window is fixed)
   - `GameViewWidth` — compile-time constant
   - `ImGui::GetStyle().WindowPadding` / `ItemSpacing` — style constants, never change at runtime

   `GetWindowContentRegionMin/Max()` is **not used** because it can fluctuate when gamepad navigation changes the window's internal scroll state, causing the panel to jump.

#### Panel behaviour
- If a texture is loaded: image is scaled to fit within the panel (aspect ratio preserved, centred).
- If no texture is available yet: "No image / available" is shown centred in the panel in muted text.
- The panel has a dark fill (`#141414`, 90% opacity) and a 1 px grey border with rounded corners.
- The panel is **not a widget** — it cannot receive focus, be clicked, or be navigated to with the D-pad.

#### Text wrapping
`ImGui::PushTextWrapPos()` uses the same compile-time constants to reserve the right column, so all info text wraps cleanly without overlapping the panel at any scroll position.

### 6. Protections and improvements (code changes)
All the following changes are implemented in the files:
- [src/thumbnailfetcher.cpp](src/thumbnailfetcher.cpp)
- [src/thumbnailfetcher.hpp](src/thumbnailfetcher.hpp)

#### a. Threaded fetching
- All thumbnail loading and downloading is performed in a background thread (see the `Thread _thread` member and its initialization in the `ThumbnailFetcher` constructor in [thumbnailfetcher.cpp](src/thumbnailfetcher.cpp)).
- The main logic runs in the method: `void ThumbnailFetcher::do_request()`.
- This prevents UI freezes and allows the main program to remain responsive.

#### b. Download size limit
- There is a hard cap of 100 KB per image (`MAX_SIZE_BYTES` constant in [thumbnailfetcher.hpp](src/thumbnailfetcher.hpp)).
- If the download exceeds this size, it is aborted and the image is discarded (see the download loop in `do_request()` in [thumbnailfetcher.cpp](src/thumbnailfetcher.cpp)).

#### c. Timeout protection (NEW)
- A timeout of 8 seconds was added to the download process in `ThumbnailFetcher::do_request()` ([thumbnailfetcher.cpp](src/thumbnailfetcher.cpp)).
- If the download takes longer than 8 seconds, it is aborted and no image is saved.
- This is implemented using `std::chrono` to measure elapsed time inside the download loop.
- The timeout check is performed on every chunk read from the network (see the `while (true)` loop in `do_request()`).

#### d. Abort support
- The `_abort` flag allows the operation to be cancelled (see the `_abort` member in [thumbnailfetcher.hpp](src/thumbnailfetcher.hpp)).
- The abort flag is checked before and during the download loop in `do_request()`.
- The destructor of `ThumbnailFetcher` sets `_abort = true` and calls `abort()` on the HTTP object if needed, then joins the thread to ensure clean shutdown (see `~ThumbnailFetcher()` in [thumbnailfetcher.cpp](src/thumbnailfetcher.cpp)).

#### e. Error handling
- All exceptions are caught and logged in `do_request()`.
- The HTTP object is cleaned up on error or abort.

---

## Supported format
- Only JPEG files are supported.
- The filename must match the game's Title ID (e.g., `PCSG00001.jpg`).

---

## Serving thumbnails over HTTP

To enable automatic downloading of thumbnails from the internet, you need to host the JPEG files on a web server that the Vita can reach over WiFi.

### File naming convention
Every file must be named after the game's **Title ID** with a `.jpg` extension, in uppercase:
```
PCSG00001.jpg
PCSB01234.jpg
NPUB30000.jpg
```

### Setting up a simple HTTP server

#### Option A — Static file hosting (recommended)
Upload your JPEG files to any static file host (nginx, Apache, GitHub Pages, Cloudflare Pages, AWS S3, etc.) under a common prefix. The Vita will request:
```
{thumbnail_url}/{TITLEID}.jpg
```
For example, if `thumbnail_url = https://myserver.com/pkgj-thumbs`, the Vita will fetch:
```
https://myserver.com/pkgj-thumbs/PCSG00001.jpg
```

#### Option B — Local network server (LAN only)
On Linux/macOS, a one-liner serves the current directory:
```bash
# Python 3
cd /path/to/your/jpgs
python3 -m http.server 8080
```
Then in `config.txt` on the Vita:
```
thumbnail_url http://192.168.1.100:8080
```
Replace `192.168.1.100` with your PC's LAN IP address.

#### Option C — Existing community sources
Some community members host PS Vita cover art archives. Search for "PS Vita cover art pack Title ID JPEG" to find ready-made archives you can self-host. There is no official public URL bundled with PKGj — you must supply your own.

### config.txt example
```
thumbnail_folder ux0:pkgj/thumbnails
thumbnail_url https://myserver.com/pkgj-thumbs
```

### Caching behaviour
Once a thumbnail is downloaded successfully it is saved to `thumbnail_folder` as `{TITLEID}.jpg`. On subsequent launches PKGj loads it from local storage directly — no network request is made again for that title.

### Limits enforced by PKGj
| Limit | Value |
|---|---|
| Max file size | 100 KB |
| Download timeout | 8 seconds |
| Format | JPEG only |

Files larger than 100 KB or downloads taking longer than 8 seconds are silently discarded — no image is saved and the panel shows "No image available".

---

## Summary of code changes
- Added an 8-second timeout to the thumbnail download loop in `ThumbnailFetcher::do_request()`.
- Ensured all network and file operations are performed in a background thread.
- Enforced a 100 KB maximum file size for thumbnails.
- Improved abort and error handling for robustness.
- Moved thumbnail display from the Personal Notes section to a **fixed framed panel** in the top-right corner of the game info window.
- Implemented the panel using `GetForegroundDrawList()` with compile-time anchors to guarantee scroll-immunity and inability to receive gamepad focus.

---

**Tip:**
To manually add thumbnails without a server, copy your JPEG files to `ux0:pkgj/thumbnails/` via USB (VitaShell), named after the Title ID (e.g. `PCSG00001.jpg`).
