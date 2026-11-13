// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Shapes
import CalcA

// Square button with a stroked SVG-path icon (20×20 view box).
Item {
    id: b

    property string path
    property bool active: true
    property color hoverColor: Theme.white
    property color hoverFill: Theme.hoverFill
    signal clicked()

    implicitWidth: 38
    implicitHeight: 34
    opacity: active ? 1 : 0.25

    Rectangle {
        anchors.fill: parent
        color: ma.containsMouse && b.active ? b.hoverFill : "transparent"
    }

    Shape {
        anchors.centerIn: parent
        width: 20; height: 20
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: ma.containsMouse && b.active ? b.hoverColor : Theme.text
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: b.path }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        enabled: b.active
        onClicked: b.clicked()
    }
}
