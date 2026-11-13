// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtCore
import CalcA

Window {
    id: win

    width: 380
    height: 620
    minimumWidth: 320
    minimumHeight: 540
    visible: true
    title: "CalcA"
    color: Theme.bg2

    readonly property bool wide: width >= 640
    readonly property int sideWidth: 300
    property bool sideOpen: false
    property bool menuOpen: false
    property string popup: "" // "", "trig", "func"
    property string sideTab: "hist"

    readonly property string iconMenu: "M 3 5 L 17 5 M 3 10 L 17 10 M 3 15 L 17 15"
    readonly property string iconHistory: "M 3.5 10 A 6.5 6.5 0 1 0 5.5 5.3 M 3 3.2 L 3 6.5 L 6.3 6.5 M 10 6.5 L 10 10 L 12.5 11.8"
    readonly property string iconTrash: "M 4 6 L 16 6 M 8 6 L 8 4 L 12 4 L 12 6 M 6 6 L 6.8 16 L 13.2 16 L 14 6"

    component Bracket: Item {
        id: br
        property color c
        property bool flipX
        property bool flipY
        width: 18; height: 18
        Rectangle { width: 18; height: 2; color: br.c; y: br.flipY ? 16 : 0 }
        Rectangle { width: 2; height: 18; color: br.c; x: br.flipX ? 16 : 0 }
    }

    component MemButton: Rectangle {
        id: mb
        property string text
        readonly property bool hovered: mbMa.containsMouse
        signal clicked()
        width: 40; height: 24
        color: mbMa.containsMouse ? "#1f05d9e8" : "transparent"
        border.color: "#4d05d9e8"
        Text {
            anchors.centerIn: parent
            text: mb.text
            color: mbMa.containsMouse ? Theme.white : Theme.text
            font.family: Theme.num
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
        MouseArea { id: mbMa; anchors.fill: parent; hoverEnabled: true; onClicked: mb.clicked() }
    }

    Controller { id: calc; objectName: "calc" }

    Settings {
        id: settings
        property alias windowWidth: win.width
        property alias windowHeight: win.height
        property string mode: "std"
        property string base: "DEC"
        property string angle: "DEG"
    }

    Component.onCompleted: {
        calc.setMode(settings.mode)
        calc.setBase(settings.base)
        calc.setAngle(settings.angle)
        keyCatcher.forceActiveFocus()
    }
    Connections {
        target: calc
        function onModeChanged() { settings.mode = calc.mode }
        function onBaseChanged() { settings.base = calc.base }
        function onAngleChanged() { settings.angle = calc.angle }
    }
    onWideChanged: if (wide) sideOpen = false

    function closeOverlays() { sideOpen = false; menuOpen = false; popup = "" }
    function openSide(tab) { sideTab = tab; sideOpen = true }
    function press(a) {
        calc.act(a)
        if (a.startsWith("t:") || a.startsWith("f:")) popup = ""
    }
    function flashKey(a) {
        for (const r of [keyRepeater, hexRepeater]) {
            for (let i = 0; i < r.count; ++i) {
                const it = r.itemAt(i)
                if (it && (it.action === a || ((a === "ce" || a === "clr") && it.action === "clrce"))) { it.flash(); return }
            }
        }
    }

    // ---------- keyboard ----------
    Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true
        Keys.onPressed: (e) => {
            if (e.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) return
            const t = e.text
            let a = ""
            if (/^[0-9]$/.test(t)) a = "d:" + t
            else if (calc.base === "HEX" && /^[a-fA-F]$/.test(t)) a = "d:" + t.toUpperCase()
            else if (t === "," || t === ".") a = "dot"
            else if (t === "+" || t === "-" || t === "*" || t === "/") a = "o:" + t
            else if (t === "^") a = "o:^"
            else if (t === "(") a = "lp"
            else if (t === ")") a = "rp"
            else if (t === "%") a = "pct"
            else if (t === "!") a = "u:fact"
            else if (t === "=" || e.key === Qt.Key_Return || e.key === Qt.Key_Enter) a = "eq"
            else if (e.key === Qt.Key_Backspace) a = "bs"
            else if (e.key === Qt.Key_Delete) a = "ce"
            else if (e.key === Qt.Key_Escape) {
                if (win.menuOpen || win.sideOpen || win.popup !== "") { win.closeOverlays(); e.accepted = true; return }
                a = "clr"
            }
            if (a === "") return
            if (calc.mode === "std" && /^(lp|rp|o:\^|u:fact)$/.test(a)) return
            if (calc.mode === "sci" && a === "pct") return
            e.accepted = true
            win.press(a)
            win.flashKey(a)
        }
    }
    Shortcut { sequences: [StandardKey.Copy]; onActivated: Engine.copyText(calc.error !== "" ? calc.errorText : calc.displayText) }
    Shortcut { sequences: [StandardKey.Paste]; onActivated: calc.paste(Engine.clipboardText()) }

    // ---------- main area ----------
    Item {
        id: mainArea
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: win.wide ? win.width - win.sideWidth : win.width

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout { // header
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                spacing: 6
                IconButton { path: win.iconMenu; onClicked: win.menuOpen = !win.menuOpen }
                Text {
                    Layout.fillWidth: true
                    text: calc.mode === "std" ? qsTr("Standard") : qsTr("Scientific")
                    elide: Text.ElideRight
                    color: Theme.white
                    font.family: Theme.head
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    font.letterSpacing: 2.6
                    font.capitalization: Font.AllUppercase
                }
                BarButton {
                    implicitWidth: 60
                    implicitHeight: 34
                    text: calc.base
                    on: calc.base !== "DEC"
                    framed: true
                    family: Theme.head
                    pixelSize: 11
                    letterSpacing: 1.6
                    onClicked: calc.cycleBase()
                }
                IconButton {
                    path: win.iconHistory
                    opacity: win.wide ? 0 : 1
                    active: !win.wide
                    onClicked: win.sideOpen && win.sideTab === "hist" ? win.closeOverlays() : win.openSide("hist")
                }
            }

            Item { // display
                Layout.fillWidth: true
                Layout.preferredHeight: 122
                Text {
                    id: exprLabel
                    anchors { left: parent.left; right: parent.right; bottom: valueLabel.top; leftMargin: 18; rightMargin: 18 }
                    text: calc.exprText
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideLeft
                    color: Theme.dim
                    font.family: Theme.mono
                    font.pixelSize: 14
                    font.letterSpacing: 0.5
                }
                Text {
                    id: valueLabel
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 18; rightMargin: 18; bottomMargin: 8 }
                    height: 72
                    text: calc.error !== "" ? calc.errorText : calc.displayText
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignBottom
                    fontSizeMode: Text.Fit
                    minimumPixelSize: 14
                    wrapMode: calc.base === "BIN" && calc.error === "" ? Text.WrapAnywhere : Text.NoWrap
                    maximumLineCount: 2
                    color: calc.error !== "" ? Theme.magenta : Theme.white
                    font.family: Theme.num
                    font.weight: calc.error !== "" ? Font.Medium : Font.DemiBold
                    font.pixelSize: calc.error !== "" ? 30 : (calc.mode === "std" ? 60 : 52)
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: calc.error !== "" ? "#66ff2a6d" : "#5905d9e8"
                        shadowBlur: 0.7
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 0
                    }
                }
                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 18; rightMargin: 18 }
                    height: 1
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: 0.3; color: "#8c05d9e8" }
                        GradientStop { position: 0.7; color: "#8cff2a6d" }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }
            }

            RowLayout { // DEG / F-E
                visible: calc.mode === "sci"
                Layout.fillWidth: true
                Layout.topMargin: 6
                Layout.leftMargin: 8
                spacing: 2
                BarButton {
                    implicitWidth: 60
                    text: calc.angle
                    family: Theme.head
                    pixelSize: 11
                    letterSpacing: 1.6
                    onClicked: calc.cycleAngle()
                }
                BarButton {
                    text: "F-E"
                    on: calc.fe
                    family: Theme.head
                    pixelSize: 11
                    letterSpacing: 1.6
                    onClicked: calc.toggleFe()
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout { // memory
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                spacing: 2
                Repeater {
                    model: [["MC", "mc"], ["MR", "mr"], ["M+", "mp"], ["M−", "mm"], ["MS", "ms"], ["M▾", "list"]]
                    BarButton {
                        required property var modelData
                        Layout.fillWidth: true
                        visible: !(modelData[1] === "list" && win.wide)
                        text: modelData[0]
                        active: calc.error === "" && (calc.memory.length > 0 || !["mc", "mr", "list"].includes(modelData[1]))
                        onClicked: modelData[1] === "list" ? win.openSide("mem") : calc.memoryAction(modelData[1])
                    }
                }
            }

            Item { // trigonometry / function drop-downs
                id: fnBar
                visible: calc.mode === "sci"
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                Row {
                    x: 8; y: 2
                    spacing: 4
                    BarButton {
                        implicitHeight: 32
                        text: "△  " + qsTr("Trigonometry") + "  ▾"
                        family: Theme.head
                        pixelSize: 10
                        letterSpacing: 1.4
                        open: win.popup === "trig"
                        onClicked: win.popup = win.popup === "trig" ? "" : "trig"
                    }
                    BarButton {
                        implicitHeight: 32
                        text: "ƒ  " + qsTr("Function") + "  ▾"
                        family: Theme.head
                        pixelSize: 10
                        letterSpacing: 1.4
                        open: win.popup === "func"
                        onClicked: win.popup = win.popup === "func" ? "" : "func"
                    }
                }
                MouseArea { // click outside closes the drop-down
                    visible: win.popup !== ""
                    parent: mainArea
                    anchors.fill: parent
                    z: 9
                    onClicked: win.popup = ""
                }
                Rectangle {
                    visible: win.popup !== ""
                    parent: mainArea
                    x: 8; y: fnBar.y + 36
                    z: 10
                    width: popGrid.width + 12
                    height: popGrid.height + 12
                    color: Theme.panel
                    border.color: Theme.line
                    MouseArea { anchors.fill: parent } // swallow clicks
                    Grid {
                        id: popGrid
                        x: 6; y: 6
                        columns: 3
                        spacing: 3
                        Repeater {
                            model: win.popup === "trig"
                                   ? [["2<sup>nd</sup>", "tinv", calc.trigInv], ["hyp", "thyp", calc.hyp], ["", "", false]]
                                     .concat(calc.trigKeys.map(k => [k[0], k[1], false]))
                                   : [["|x|", "f:abs", false], ["⌊x⌋", "f:floor", false], ["⌈x⌉", "f:ceil", false],
                                      ["rand", "f:rand", false], ["→dms", "f:dms", false], ["→deg", "f:deg", false]]
                            Key {
                                required property var modelData
                                width: 64; height: 46
                                visible: modelData[1] !== ""
                                label: modelData[0]
                                action: modelData[1]
                                on: modelData[2]
                                fontSize: 17
                                onTriggered: (a) => win.press(a)
                            }
                        }
                    }
                }
            }

            GridLayout { // A-F
                visible: calc.base === "HEX"
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                Layout.topMargin: 4
                columns: 6
                columnSpacing: 3
                Repeater {
                    id: hexRepeater
                    model: ["A", "B", "C", "D", "E", "F"]
                    Key {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        label: modelData
                        action: "d:" + modelData
                        kind: "d"
                        fontSize: 21
                        onTriggered: (a) => win.press(a)
                    }
                }
            }

            GridLayout { // keypad
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 6
                Layout.topMargin: 4
                columns: calc.mode === "std" ? 4 : 5
                rowSpacing: 3
                columnSpacing: 3
                Repeater {
                    id: keyRepeater
                    model: calc.keys
                    Key {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        label: modelData[1] === "clrce" ? (calc.canClearEntry ? "CE" : "C") : modelData[0]
                        action: modelData[1]
                        kind: modelData[2]
                        on: modelData[1] === "2nd" && calc.second
                        active: calc.keyEnabled(modelData[1], modelData[3]) && (calc.error === "" || modelData[3]) && calc.base.length > 0
                        onTriggered: (a) => win.press(a)
                    }
                }
            }
        }
    }

    // ---------- scrim for narrow overlays ----------
    Rectangle {
        anchors.fill: parent
        z: 30
        color: "#99020409"
        visible: win.menuOpen || (win.sideOpen && !win.wide)
        MouseArea { anchors.fill: parent; onClicked: win.closeOverlays() }
    }

    // ---------- history / memory ----------
    Rectangle {
        id: side
        z: 35
        x: win.wide ? win.width - win.sideWidth : 0
        width: win.wide ? win.sideWidth : win.width
        height: win.wide ? win.height : Math.round(win.height * 0.66)
        y: win.wide ? 0 : (win.sideOpen ? win.height - height : win.height + 2)
        color: Theme.side
        Behavior on y { enabled: !win.wide; NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

        Rectangle { // separator
            width: win.wide ? 1 : parent.width
            height: win.wide ? parent.height : 1
            color: win.wide ? "#2905d9e8" : Theme.line
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Row {
                Layout.topMargin: 8
                Layout.leftMargin: 10
                spacing: 4
                Repeater {
                    model: [["hist", qsTr("History")], ["mem", qsTr("Memory")]]
                    Item {
                        required property var modelData
                        width: tabLabel.implicitWidth + 24
                        height: 36
                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: modelData[1]
                            color: win.sideTab === modelData[0] ? Theme.white : (tabMa.containsMouse ? Theme.text : Theme.dim)
                            font.family: Theme.head
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 2
                            font.capitalization: Font.AllUppercase
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 2
                            color: Theme.magenta
                            visible: win.sideTab === modelData[0]
                        }
                        MouseArea { id: tabMa; anchors.fill: parent; hoverEnabled: true; onClicked: win.sideTab = modelData[0] }
                    }
                }
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: win.sideTab === "hist" ? calc.history : calc.memory
                delegate: Item {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool isHist: win.sideTab === "hist"
                    width: list.width
                    height: col.implicitHeight + 20
                    Rectangle { anchors.fill: parent; color: rowMa.containsMouse ? "#0f05d9e8" : "transparent" }
                    MouseArea {
                        id: rowMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (row.isHist) calc.recallHistory(row.index); else calc.recallMemory(row.index)
                            if (!win.wide) win.sideOpen = false
                        }
                    }
                    Column {
                        id: col
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 18; rightMargin: 18 }
                        spacing: 2
                        Text {
                            visible: row.isHist
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideLeft
                            text: row.isHist ? row.modelData.expr : ""
                            color: Theme.dim
                            font.family: Theme.mono
                            font.pixelSize: 13
                        }
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideLeft
                            text: row.isHist ? row.modelData.text : row.modelData
                            color: Theme.white
                            font.family: Theme.num
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                        }
                        Row {
                            visible: !row.isHist
                            opacity: rowMa.containsMouse || memMc.hovered || memMp.hovered || memMm.hovered ? 1 : 0
                            anchors.right: parent.right
                            spacing: 4
                            MemButton { id: memMc; text: "MC"; onClicked: calc.memoryItem(row.index, "mc") }
                            MemButton { id: memMp; text: "M+"; onClicked: calc.memoryItem(row.index, "mp") }
                            MemButton { id: memMm; text: "M−"; onClicked: calc.memoryItem(row.index, "mm") }
                        }
                    }
                }
                Text {
                    visible: list.count === 0
                    x: 18; y: 8
                    width: list.width - 36
                    wrapMode: Text.WordWrap
                    text: win.sideTab === "hist" ? qsTr("There's no history yet.") : qsTr("There's nothing saved in memory.")
                    color: Theme.dim
                    font.family: Theme.num
                    font.pixelSize: 15
                    font.weight: Font.Medium
                }
            }

            IconButton {
                Layout.alignment: Qt.AlignRight
                Layout.rightMargin: 22
                Layout.bottomMargin: 22
                path: win.iconTrash
                active: list.count > 0
                hoverColor: Theme.magenta
                hoverFill: "#1aff2a6d"
                onClicked: win.sideTab === "hist" ? calc.clearHistory() : calc.clearMemory()
            }
        }
    }

    // ---------- mode menu ----------
    Rectangle {
        z: 50
        width: 250
        height: parent.height
        x: win.menuOpen ? 0 : -width - 2
        color: Theme.panel
        Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }
        MouseArea { anchors.fill: parent }
        Column {
            anchors.fill: parent
            anchors.margins: 8
            Item { width: 1; height: 50 }
            Text {
                leftPadding: 12
                bottomPadding: 10
                text: "CALCA // " + qsTr("Mode").toUpperCase()
                color: Theme.teal
                font.family: Theme.mono
                font.pixelSize: 11
                font.letterSpacing: 2.2
            }
            Repeater {
                model: [["std", "±", qsTr("Standard")], ["sci", "ƒ", qsTr("Scientific")]]
                Item {
                    required property var modelData
                    width: parent.width
                    height: 42
                    Rectangle { anchors.fill: parent; color: miMa.containsMouse ? "#1405d9e8" : "transparent" }
                    Rectangle {
                        visible: calc.mode === modelData[0]
                        x: 0; y: 8
                        width: 3; height: parent.height - 16
                        color: Theme.magenta
                    }
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 12
                        spacing: 14
                        Text {
                            width: 22
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData[1]
                            color: Theme.cyan
                            font.family: Theme.num
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData[2]
                            color: calc.mode === modelData[0] || miMa.containsMouse ? Theme.white : Theme.text
                            font.family: Theme.head
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.8
                            font.capitalization: Font.AllUppercase
                        }
                    }
                    MouseArea {
                        id: miMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { calc.setMode(modelData[0]); win.menuOpen = false; win.popup = "" }
                    }
                }
            }
        }
    }

    // ---------- corner brackets ----------
    Item {
        anchors.fill: parent
        z: 100
        Bracket { c: Theme.cyan; anchors.left: parent.left; anchors.top: parent.top }
        Bracket { c: Theme.cyan; flipX: true; anchors.right: parent.right; anchors.top: parent.top }
        Bracket { c: Theme.magenta; flipY: true; anchors.left: parent.left; anchors.bottom: parent.bottom }
        Bracket { c: Theme.magenta; flipX: true; flipY: true; anchors.right: parent.right; anchors.bottom: parent.bottom }
    }
}
