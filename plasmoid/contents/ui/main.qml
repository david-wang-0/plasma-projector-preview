// SPDX-FileCopyrightText: 2026 David Wang
// SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
//
// Live mirror of another output, for keeping an eye on a projector from the laptop.
// KWin streams the output over zkde_screencast (as for task manager thumbnails), asked for
// by our OutputStream plugin (local.outputstream) with the mouse pointer embedded, and the
// frames stay on the GPU, so scaling the window costs nothing. KWin only grants that
// protocol to trusted clients, so this runs as a widget: plasmawindowed local.projectorpreview
// (bin/projector-preview). Shows the first screen other than its own; click to cycle
// through the screens.
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

        Rectangle { anchors.fill: parent; color: "black" }

        OutputStream {
            id: cast
            outputName: view.output  // a QObject has no parent property, hence the id
        }

        PipeWire.PipeWireSourceItem {
            anchors.fill: parent
            nodeId: cast.nodeId
            visible: nodeId !== 0
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
        }
    }
}
