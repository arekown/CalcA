// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
import QtQuick
import QtQuick.Shapes
import CalcA

// Calculator key with a chamfered corner.
Item {
    id: k

    property string label
    property string action
    property string kind: "fn" // fn | d | op | eq
    property bool on: false
    property bool active: true
    property bool flashing: false
    property int fontSize: kind === "d" ? 24 : kind === "op" ? 26 : kind === "eq" ? 28 : 19
    signal triggered(string action)

    readonly property int cut: 7
    readonly property bool hot: ma.containsMouse && active
    readonly property bool down: (ma.pressed && active) || flashing

    function flash() { flashing = true; flashTimer.restart() }

    opacity: active ? 1 : 0.25

    Timer { id: flashTimer; interval: 110; onTriggered: k.flashing = false }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: k.on || (k.hot && k.kind !== "eq") ? 1 : -1
            strokeColor: k.on ? Theme.yellow : Theme.cyanHover
            fillColor: k.down ? (k.kind === "eq" ? Theme.magenta : Theme.press)
                     : k.kind === "eq" ? "transparent"
                     : k.kind === "d" ? (k.hot ? Theme.keyHover : Theme.key)
                     : (k.hot ? Theme.keyFnHover : Theme.keyFn)
            fillGradient: k.kind === "eq" && !k.down ? eqGradient : null
            startX: k.kind === "eq" ? k.cut : 0
            startY: 0
            PathLine { x: k.width; y: 0 }
            PathLine { x: k.width; y: k.height - k.cut }
            PathLine { x: k.width - k.cut; y: k.height }
            PathLine { x: 0; y: k.height }
            PathLine { x: 0; y: k.kind === "eq" ? k.cut : 0 }
            PathLine { x: k.kind === "eq" ? k.cut : 0; y: 0 }
        }
    }

    property LinearGradient eqGradient: LinearGradient {
        x1: 0; y1: 0; x2: k.width; y2: k.height
        GradientStop { position: 0; color: k.hot ? "#ff4a85" : Theme.magenta }
        GradientStop { position: 1; color: k.hot ? "#d42a62" : Theme.magentaDark }
    }

    Text {
        visible: k.action !== "bs"
        anchors.centerIn: parent
        text: k.label
        textFormat: Text.RichText
        color: k.on ? Theme.yellow
             : k.kind === "op" ? Theme.cyan
             : k.kind === "eq" ? "#ffffff"
             : (k.kind === "d" || k.hot || k.down) ? Theme.white : Theme.text
        font.family: Theme.num
        font.pixelSize: k.fontSize
        font.weight: k.kind === "eq" ? Font.Bold : (k.kind === "fn" ? Font.Medium : Font.DemiBold)
    }

    Shape { // backspace symbol
        visible: k.action === "bs"
        anchors.centerIn: parent
        width: 24; height: 24
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: k.hot || k.down ? Theme.white : Theme.text
            strokeWidth: 1.6
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M 8 5 L 20 5 L 20 19 L 8 19 L 3 12 Z M 11 9 L 17 15 M 17 9 L 11 15" }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        enabled: k.active
        onClicked: k.triggered(k.action)
    }
}
