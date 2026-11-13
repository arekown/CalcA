// SPDX-License-Identifier: GPL-2.0-or-later
import QtQuick
import CalcA
import "logic.js" as Logic

// Bridges the JS state machine (logic.js) to bindable properties.
QtObject {
    id: c

    property string mode: "std"
    property string base: "DEC"
    property string angle: "DEG"
    property bool fe: false
    property bool second: false
    property bool trigInv: false
    property bool hyp: false
    property string displayText: "0"
    property string exprText: ""
    property string error: ""
    property bool canClearEntry: false
    property var history: []
    property var memory: []
    property var keys: []
    property var trigKeys: []

    // Connect the engine while the object is being created: property initialisers of all objects
    // run before any Component.onCompleted, whose order between objects is not defined.
    readonly property bool engineReady: { Logic.init(Engine); return true }

    readonly property string errorText: error === "div0" ? qsTr("Cannot divide by zero")
                                      : error === "overflow" ? qsTr("Overflow")
                                      : error === "timeout" ? qsTr("Calculation takes too long")
                                      : error !== "" ? qsTr("Invalid input") : ""

    function act(a) { Logic.act(a); refresh() }
    function setMode(m) { Logic.setMode(m); refresh() }
    function cycleBase() { Logic.cycleBase(); refresh() }
    function setBase(b) { Logic.setBase(b); refresh() }
    function cycleAngle() { Logic.cycleAngle(); refresh() }
    function setAngle(a) { Logic.setAngle(a); refresh() }
    function toggleFe() { Logic.toggleFe(); refresh() }
    function memoryAction(m) { Logic.memory(m); refresh() }
    function memoryItem(i, m) { Logic.memoryItem(i, m); refresh() }
    function recallMemory(i) { Logic.recallMemory(i); refresh() }
    function recallHistory(i) { Logic.recallHistory(i); refresh() }
    function clearHistory() { Logic.clearHistory(); refresh() }
    function clearMemory() { Logic.clearMemory(); refresh() }
    function paste(text) { const ok = Logic.paste(text); refresh(); return ok }
    function keyEnabled(a, keep) { return Logic.keyEnabled(a, keep) }

    function refresh() {
        const v = Logic.view()
        displayText = v.display
        exprText = v.expr
        error = v.error
        canClearEntry = v.canClearEntry
        history = v.hist
        memory = v.mem
        const keysChanged = mode !== v.mode || second !== v.second || keys.length === 0
        mode = v.mode
        base = v.base
        angle = v.angle
        fe = v.fe
        second = v.second
        if (keysChanged) keys = Logic.keyDefs()
        if (trigInv !== v.trigInv || hyp !== v.hyp || trigKeys.length === 0) {
            trigInv = v.trigInv
            hyp = v.hyp
            trigKeys = Logic.trigDefs()
        }
    }

    Component.onCompleted: refresh()
}
