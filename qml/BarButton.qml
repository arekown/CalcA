// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import CalcA

// Flat text button (memory bar, DEG, F-E, DEC/HEX/BIN, drop-down buttons, tabs).
Item {
    id: b

    property string text
    property bool on: false
    property bool active: true
    property bool framed: false
    property bool open: false
    property string family: Theme.num
    property int pixelSize: 14
    property real letterSpacing: 0.8
    signal clicked()

    implicitWidth: label.implicitWidth + 20
    implicitHeight: 30
    opacity: active ? 1 : 0.45

    Rectangle {
        anchors.fill: parent
        color: (ma.containsMouse && b.active) || b.open ? Theme.hoverFill : "transparent"
        border.width: b.framed ? 1 : 0
        border.color: b.on ? Theme.cyan : "#4005d9e8"
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: b.text
        color: !b.active ? Theme.dim : b.on ? Theme.cyan : ((ma.containsMouse || b.open) ? Theme.white : Theme.text)
        font.family: b.family
        font.pixelSize: b.pixelSize
        font.weight: Font.DemiBold
        font.letterSpacing: b.letterSpacing
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        enabled: b.active
        onClicked: b.clicked()
    }
}
