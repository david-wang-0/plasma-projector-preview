// SPDX-FileCopyrightText: 2026 David Wang
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
//
// Live mirror of another output, for keeping an eye on a projector from the laptop.
// KWin streams the output over zkde_screencast (as for task manager thumbnails), asked for
// by our OutputStream plugin (local.outputstream) with the mouse pointer embedded, and the
// frames stay on the GPU, so scaling the window costs nothing. KWin only grants that
// protocol to trusted clients, so this runs as a widget: plasmawindowed local.projectorpreview
// (bin/projector-preview). Shows the first screen other than its own; click to cycle
// through the screens. Ctrl+wheel zooms in; while zoomed, the view follows the pointer on the
// mirrored screen, whose position a KWin script sends us (CursorTracker).
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.plasma.plasmoid
import org.kde.pipewire as PipeWire
import local.outputstream

PlasmoidItem {
    id: root
    preferredRepresentation: fullRepresentation

    fullRepresentation: Item {
        id: view
        Layout.preferredWidth: 640
        Layout.preferredHeight: 360

        property int pick: 0  // clicks so far, advances through the other screens
        readonly property var others: Qt.application.screens.map(s => s.name)
                                        .filter(n => n !== Screen.name)
        readonly property string output: others.length ? others[pick % others.length] : ""
        // The mirrored screen's place on the desktop (logical pixels), to follow the pointer.
        readonly property var outputScreen: Qt.application.screens.find(s => s.name === output) ?? null

        onOutputChanged: resetZoom()

        // Zoom. At 1 the whole screen is fitted into the window. The view is described by the
        // picture point at the window's centre, normalised to 0..1 across the picture, so it
        // survives resizing. Shown values approach the targets frame by frame (FrameAnimation).
        readonly property real maxZoom: 8
        property real zoom: 1
        property real zoomTarget: 1
        property real cx: 0.5
        property real cy: 0.5
        property real targetCx: 0.5
        property real targetCy: 0.5
        // While a Ctrl+wheel zoom animates: the picture point {u, v} that stays under the
        // window point {x, y} the wheel turned at; null otherwise.
        property var anchor: null

        // The aspect-fit rectangle the stream is painted into at zoom 1 (what
        // PipeWireSourceItem.paintedRect reports, but that is only updated on painting, a
        // frame late, so compute it).
        readonly property size stream: source.streamSize.width > 0 ? source.streamSize
                                                                    : Qt.size(width, height)
        readonly property real fitScale: Math.min(width / stream.width, height / stream.height)
        readonly property rect fit: Qt.rect((width - stream.width * fitScale) / 2,
                                            (height - stream.height * fitScale) / 2,
                                            stream.width * fitScale, stream.height * fitScale)
        // The centre actually shown: kept where the zoomed picture still covers the window.
        readonly property real shownCx: clampCentre(cx, zoom, width, fit.width)
        readonly property real shownCy: clampCentre(cy, zoom, height, fit.height)

        // Centre c along one axis, limited so that at zoom z the picture (extent `picture` at
        // zoom 1) leaves no empty margin in the window (extent `extent`). A picture still
        // narrower than the window on that axis stays centred.
        function clampCentre(c, z, extent, picture) {
            const half = extent / (2 * z * picture)
            return half >= 0.5 ? 0.5 : Math.min(Math.max(c, half), 1 - half)
        }

        function resetZoom() {
            anchor = null
            zoom = zoomTarget = 1
            cx = cy = targetCx = targetCy = 0.5
        }

        // While zoomed in, centre the pointer's spot if the pointer is on the mirrored screen.
        // Elsewhere (e.g. over this window, on the laptop) the view stays put.
        function follow() {
            const s = outputScreen
            if (zoomTarget <= 1 || !tracker.valid || !s) {
                return
            }
            const u = (tracker.position.x - s.virtualX) / s.width
            const v = (tracker.position.y - s.virtualY) / s.height
            if (u < 0 || u >= 1 || v < 0 || v >= 1) {
                return
            }
            anchor = null
            targetCx = clampCentre(u, zoomTarget, width, fit.width)
            targetCy = clampCentre(v, zoomTarget, height, fit.height)
        }

        // Ctrl+wheel: zoom by `notches` (1.25 per notch) around the window point (x, y).
        function zoomAt(notches, x, y) {
            const z = Math.min(Math.max(zoomTarget * Math.pow(1.25, notches), 1), maxZoom)
            if (z === zoomTarget) {
                return
            }
            // The picture point now under (x, y), from what is shown.
            const u = (x - source.x) / zoom / fit.width - fit.x / fit.width
            const v = (y - source.y) / zoom / fit.height - fit.y / fit.height
            anchor = { u: u, v: v, x: x, y: y }
            zoomTarget = z
            targetCx = clampCentre(u - (x - width / 2) / (z * fit.width), z, width, fit.width)
            targetCy = clampCentre(v - (y - height / 2) / (z * fit.height), z, height, fit.height)
            follow()
        }

        CursorTracker {
            id: tracker
            onPositionChanged: view.follow()
        }

        FrameAnimation {
            running: view.anchor !== null || view.zoom !== view.zoomTarget
                     || view.cx !== view.targetCx || view.cy !== view.targetCy
            onTriggered: {
                // Exponential approach, independent of the frame rate: the zoom quickly (it
                // answers the wheel), the pan more gently (it smooths the pointer's jitter).
                const zoomStep = 1 - Math.exp(-frameTime * 25)
                const panStep = 1 - Math.exp(-frameTime * 12)
                let z = Math.exp(Math.log(view.zoom)
                                 + (Math.log(view.zoomTarget) - Math.log(view.zoom)) * zoomStep)
                if (Math.abs(z / view.zoomTarget - 1) < 1e-3) {
                    z = view.zoomTarget
                }
                view.zoom = z
                const a = view.anchor
                if (a) {
                    // Keep the anchored picture point under the wheel's window point.
                    view.cx = view.clampCentre(a.u - (a.x - view.width / 2) / (z * view.fit.width),
                                               z, view.width, view.fit.width)
                    view.cy = view.clampCentre(a.v - (a.y - view.height / 2) / (z * view.fit.height),
                                               z, view.height, view.fit.height)
                    if (z === view.zoomTarget) {
                        view.cx = view.targetCx
                        view.cy = view.targetCy
                        view.anchor = null
                    }
                    return
                }
                // Snap once less than a tenth of a screen pixel away, so the animation stops.
                const snap = 0.1 / (z * Math.max(view.fit.width, view.fit.height, 1))
                const dx = view.targetCx - view.cx
                const dy = view.targetCy - view.cy
                view.cx = Math.abs(dx) < snap ? view.targetCx : view.cx + dx * panStep
                view.cy = Math.abs(dy) < snap ? view.targetCy : view.cy + dy * panStep
            }
        }

        Rectangle { anchors.fill: parent; color: "black" }

        OutputStream {
            id: cast
            outputName: view.output  // a QObject has no parent property, hence the id
        }

        Item {  // the window onto the (zoomed) picture
            anchors.fill: parent
            clip: true

            PipeWire.PipeWireSourceItem {
                id: source
                // Zooming enlarges the item and moves it so that the picture point
                // (shownCx, shownCy) lands in the window's centre.
                width: view.width * view.zoom
                height: view.height * view.zoom
                x: view.width / 2 - view.zoom * (view.fit.x + view.shownCx * view.fit.width)
                y: view.height / 2 - view.zoom * (view.fit.y + view.shownCy * view.fit.height)
                nodeId: cast.nodeId
                visible: nodeId !== 0

                // Shrinking a big screen straight into the window samples only some of its
                // pixels, so text turns jagged and shimmers. When shrinking (more stream pixels
                // than painted pixels), draw into a full-resolution layer first and sample that
                // through mipmaps (averaged).
                layer.enabled: streamSize.width > view.fit.width * view.zoom * Screen.devicePixelRatio
                layer.textureSize: streamSize
                layer.mipmap: true
                layer.smooth: true
            }
        }

        Text {
            anchors.centerIn: parent
            visible: cast.nodeId === 0
            color: "white"
            text: parent.output ? "Waiting for " + parent.output + " …" : "No other screen"
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.pick++

            // Ctrl+wheel zooms around the hovered point; the plain wheel does nothing. (Handled
            // here: a WheelHandler inside this MouseArea never receives the events.)
            onWheel: wheel => {
                if (!(wheel.modifiers & Qt.ControlModifier)) {
                    wheel.accepted = false
                    return
                }
                // A touchpad reports pixels (smooth); a mouse wheel 120 per notch.
                const notches = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y / 100
                                                         : wheel.angleDelta.y / 120
                view.zoomAt(notches, wheel.x, wheel.y)
            }
        }
    }
}
