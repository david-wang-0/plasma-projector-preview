# plasma-projector-preview

> **Not a KDE project.** This is an independent, unofficial tool that runs on KDE Plasma. It
> is not affiliated with, endorsed by or maintained by KDE or KDE e.V.; please don't report
> its problems to KDE. KDE® and Plasma are trademarks of KDE e.V.

A live, GPU-scaled view of another screen in a small window, for KDE Plasma 6 on Wayland.
The command it installs is `projector-preview`.
Made for teaching with a projector: the projector stays a normal **extended** screen (put
windows on it with Meta+Shift+Arrow or by dragging them across the edge), and a window on
the laptop shows what the room sees, mouse pointer included.

## Usage

```sh
projector-preview
```

opens a 640×360 window showing the first screen other than the one the window is on. It
follows the window: if KWin happens to open it on the projector, it shows the laptop until
you drag it onto the laptop. Click it to cycle through the other screens. It is an ordinary
KDE window, so move and resize it freely; it runs at full frame rate at any size, because
the frames stay on the GPU. The view is view-only (clicks do not reach the mirrored
screen). The mouse pointer is drawn into the picture whenever it is on the mirrored screen.

To zoom, hold Ctrl and scroll over the window (mouse wheel or two-finger touchpad scroll), up
to 8×; it zooms around the point under the pointer. While zoomed in, the view follows the
pointer smoothly whenever it is on the mirrored screen, keeping it near the middle (as far as
the edges of the picture allow); while the pointer is elsewhere, e.g. over the preview window,
the view stays where it is. Zoom out fully (Ctrl+scroll down) to get the whole screen back;
switching screens by clicking also resets the zoom. Scrolling without Ctrl does nothing.

From the GUI: once the desktop file is linked (see below), start **Projector Preview** from
the application launcher or KRunner (Alt+Space, "projector"), pin it to the task manager, or
give it a global shortcut in System Settings → Keyboard → Shortcuts → Add New → Application.

Set the projector to *extend*, not *mirror*, in System Settings → Display Configuration.

## Requirements

- KDE Plasma 6 on Wayland (KWin), tested with Plasma 6.7 and Qt 6.11.
- To build: CMake ≥ 3.21, a C/C++ compiler, Qt 6 (≥ 6.7) development files for Core, DBus,
  Gui, Qml and WaylandClient (`qt6-wayland`), and `plasma-wayland-protocols`.
- At run time: `plasmawindowed` (from `plasma-workspace`) and the `org.kde.pipewire` QML
  module (from `kpipewire`).

Package names are Arch/Manjaro's; other distributions split them similarly (look for the
`-dev`/`-devel` variants).

## Build and install

```sh
./build.sh
```

builds the QML plugin in `plugin/` and installs it to `$HOME/.local/lib/qt6/qml/local/outputstream`
(set `PREFIX` to install under another prefix; `bin/projector-preview` looks in
`$HOME/.local/lib/qt6/qml`). It is safe to re-run. Then link the widget and the launcher:

```sh
ln -s "$PWD/plasmoid" "$HOME/.local/share/plasma/plasmoids/local.projectorpreview"
ln -s "$PWD/bin/projector-preview" "$HOME/.local/bin/projector-preview"
```

For an application-menu entry (Kickoff, KRunner, pinnable to the panel, and a global
shortcut via System Settings → Keyboard → Shortcuts → Add Application), also link the
desktop file:

```sh
ln -s "$PWD/projector-preview.desktop" "$HOME/.local/share/applications/projector-preview.desktop"
```

It runs `projector-preview` from `PATH`, so `$HOME/.local/bin` must be on the Plasma
session's `PATH` (it is by default on most distributions).

(A dotfiles repo can do the same from its install script, with this repo as a submodule.)

## How it works

- `bin/projector-preview` adds `$HOME/.local/lib/qt6/qml` to `QML_IMPORT_PATH` and runs
  `plasmawindowed local.projectorpreview`, i.e. the Plasma widget in `plasmoid/` in a window
  of its own.
- KWin streams a whole output to PipeWire over the `zkde_screencast_unstable_v1` Wayland
  protocol, the same mechanism the task manager uses for window thumbnails. KWin grants that
  protocol only to programs whose desktop file lists it in `X-KDE-Wayland-Interfaces`.
  `plasmawindowed` is one of them, which is why this is a widget and not a standalone QML
  script (`qml6` would be refused).
- `plugin/` is a small QML module, `local.outputstream`, with two types. The first:

  ```qml
  OutputStream {
      outputName: "HDMI-A-1"   // a QScreen / Screen.name
      pointer: true            // default: KWin draws the pointer into the frames
      // read-only: nodeId (PipeWire node, 0 while there is none), error
  }
  ```

  It is derived from plasma-workspace's `libtaskmanager` (`Screencasting`,
  `ScreencastingRequest`), which is not installed as a library one could link against. The
  stock `ScreencastingRequest` QML type always asks for `pointer_hidden`; `OutputStream` asks
  for `pointer_embedded` (or hidden with `pointer: false`).
- `PipeWireSourceItem` from `org.kde.pipewire` (kpipewire) shows the stream, importing the
  DMA-BUF frames on the GPU, so scaling is free.
- When the window is smaller than the mirrored screen, the item is drawn into a layer at the
  stream's full resolution and sampled through mipmaps (`layer.textureSize: streamSize`,
  `layer.mipmap: true`). Without that, shrinking samples only some of the pixels, so small
  text turns jagged and shimmers while resizing. It costs one full-size GPU copy per frame
  and is off when the picture is shown at native size or larger (window size times zoom).
- Zooming enlarges the `PipeWireSourceItem` inside a clipping item and moves it. The view is
  kept as the picture point at the window's centre, eased towards its target every frame
  (`FrameAnimation`, exponential approach) and clamped so that no empty margin shows.
- To follow the pointer, the widget needs its global position, which a Wayland client cannot
  query and which neither KWin's D-Bus API nor kpipewire offers. The plugin's second type,
  `CursorTracker` (read-only `position` in global logical coordinates, and `valid`), gets it
  from KWin: it writes a small KWin script to
  `$XDG_RUNTIME_DIR/plasma-projector-preview/cursor-<pid>.js`, loads it through KWin's
  `org.kde.kwin.Scripting` D-Bus interface as `plasma-projector-preview-cursor-<pid>` and runs
  it. The script sends `workspace.cursorPos` on every `workspace.cursorPosChanged` with
  `callDBus` to the preview's unique bus name, object `/CursorTracker`, method
  `io.github.david_wang_0.PlasmaProjectorPreview.CursorTracker.setPosition(x, y)`. `callDBus`
  sends each JS number as an `int32` when it is integral and as a `double` otherwise, so the
  receiver is a `QDBusVirtualObject` that accepts either. The widget compares the position with
  the mirrored screen's geometry from `Qt.application.screens`. When the preview exits
  normally (including on SIGTERM), it unloads the script and deletes the file. A preview killed
  hard cannot; the next preview to start unloads the scripts and deletes the files of
  previews that are no longer running.

### The start-up race

The Wayland global is bound asynchronously. A stream requested before the binding is active
is lost: `Screencasting::createOutputStream` returns null while `!isActive()`, and
`ScreencastingRequest` does not try again, so the widget used to need a timer that kept
re-setting the output name. `OutputStream` instead requests the stream on the extension's
`activeChanged`, whenever `outputName` or `pointer` changes, and when a screen is added
while it has none. The old stream is closed on every change and in the destructor.

## Checking it still works (after a Plasma or Qt update)

1. With an external screen attached, run `projector-preview`: the picture should appear
   within a second, with the pointer when it is on that screen. Ctrl+scroll over it zooms
   in, and the zoomed view should then follow the pointer on the external screen.
2. `pw-dump` should list a `Stream/Input/Video` node of `plasmawindowed` in state `running`.
3. `qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.isScriptLoaded plasma-projector-preview-cursor-<pid>`
   (the pid of `plasmawindowed`) should print `true`. If KWin's scripting API changes (e.g.
   `workspace.cursorPos` or `cursorPosChanged` renamed), only following the pointer stops;
   zooming still works. Script errors show in `journalctl --user -t kwin_wayland`.
4. `WAYLAND_DEBUG=1 timeout 5 projector-preview 2>&1 | grep stream_output` should show
   `zkde_screencast_unstable_v1#….stream_output(new id …, wl_output#…, 2)`; the last
   argument is the pointer mode (1 hidden, 2 embedded, 4 metadata).

Symptoms when something broke: the window stays at "Waiting for <output> …" (no stream), or
the widget fails to load. `plasmawindowed` logs to the systemd journal
(`journalctl --user -t plasmawindowed`), not to the terminal, so look there for QML errors
such as `Cannot load library …liboutputstream.so`.

## If the plugin breaks

- After a Qt update the plugin may fail to load, as it uses Qt's Wayland client classes,
  which carry no compatibility promise between Qt versions: run `./build.sh` again.
- If it no longer compiles, `plasmoid/contents/ui/main.qml` can go back to the stock type:
  replace `import local.outputstream` and the `OutputStream` with
  `import org.kde.taskmanager as TaskManager` and `TaskManager.ScreencastingRequest { id: cast }`,
  plus a `Timer` (300 ms, repeating while `cast.nodeId === 0`) that sets
  `cast.outputName` to `""` and back to the output, to get past the start-up race. That loses
  the pointer. Also remove the `CursorTracker` and its uses in `follow()`; zooming then works
  without following the pointer.

## Limitations

- Plasma 6 on Wayland only (KWin's private screencast protocol).
- View-only; no input goes to the mirrored screen.
- No screen picker beyond click-to-cycle, and nothing is remembered between runs.
- If a preview is killed hard (SIGKILL, crash), its KWin script stays loaded, sending
  pointer positions to a bus name that is gone, until the next preview starts (which unloads
  it) or the session ends. That is harmless.
- One `CursorTracker` per process (it owns the object path `/CursorTracker`).

## Tried first and dropped

- The xdg-desktop-portal ScreenCast API played with GStreamer (`pipewiresrc ! videoconvert !
  glimagesink`). Its software conversion and copies held it near 10 fps, and resizing the
  window stalled it at 0 fps. Feeding the DMA-BUFs straight to GTK4 (`gtk4paintablesink`)
  gave a gray window: GTK cannot import KWin's Intel CCS-compressed buffers ("Failed to
  download dmabuf texture", modifier `0x100000000000008`). It also needs a picker plus a
  restore token.
- A nested `kwin_wayland --wayland-display wayland-0 --output-count 2` with one output
  mirroring the other (`kscreen-doctor output.WL-1.mirror.WL-0` with
  `KSCREEN_BACKEND_INPROCESS=1 WAYLAND_DISPLAY=<nested>`). It works, but the nested desktop
  has no wallpaper or panel, apps must be launched into it by `WAYLAND_DISPLAY`, existing
  windows cannot move in, and resizing the scaled mirror window misbehaves.
- The stock `ScreencastingRequest` from `org.kde.taskmanager`: works, but without the
  pointer, and with a retry timer for the start-up race (see above).

## Licence

LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL, the licence of the
plasma-workspace code the plugin is derived from; see `LICENSE` and `LICENSES/`.
