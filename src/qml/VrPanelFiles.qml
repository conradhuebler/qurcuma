// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Headset panel "Open structure" (Claude Generated 2026): pages through the directory VrSession lists
// (sub-directories first, then xyz/vtf/pdb/mol2 files), like the desktop file browser dock.
//   session: VrSession (or MockVr): currentDir, entries [{name, isDir}], enter(), up(), choose()
pragma ComponentBehavior: Bound
import QtQuick
import "VrStyle.js" as Style

VrPanel {
    id: panel

    required property var session
    signal closeRequested()

    readonly property int pageSize: 6
    property int page: 0
    readonly property int entryCount: session.entries ? session.entries.length : 0
    readonly property int pageCount: Math.max(1, Math.ceil(entryCount / pageSize))
    readonly property var pageEntries: {
        var list = []
        var all = session.entries
        for (var i = page * pageSize; i < Math.min(entryCount, (page + 1) * pageSize); ++i)
            list.push(all[i])
        return list
    }

    title: "Open structure"
    width: 360

    headerItems: VrButton {
        height: 36
        text: "Close"
        onClicked: panel.closeRequested()
    }

    Connections {
        target: panel.session
        function onCurrentDirChanged() { panel.page = 0 }
    }

    Text {
        width: parent.width
        text: panel.session.currentDir
        color: Style.folder
        font.pixelSize: Style.fontSmall
        elide: Text.ElideLeft
    }

    // Fixed height for a full page, so the panel keeps its size while paging.
    Item {
        width: parent.width
        height: panel.pageSize * Style.listRowHeight + (panel.pageSize - 1) * 4

        Column {
            width: parent.width
            spacing: 4
            Repeater {
                model: panel.pageEntries
                delegate: VrButton {
                    id: row
                    required property var modelData
                    width: parent.width
                    height: Style.listRowHeight
                    leftPadding: 14
                    alignment: Text.AlignLeft
                    font.pixelSize: Style.fontBody
                    text: modelData.isDir ? "▸  " + modelData.name + "/" : modelData.name
                    onClicked: {
                        if (row.modelData.isDir) {
                            panel.session.enter(row.modelData.name)
                        } else {
                            panel.session.choose(row.modelData.name)
                            panel.closeRequested()
                        }
                    }
                }
            }
        }
        Text {
            anchors.centerIn: parent
            visible: panel.entryCount === 0
            text: "No folders or structure files here"
            color: Style.textDim
            font.pixelSize: Style.fontBody
        }
    }

    Row {
        id: pager
        spacing: Style.spacing
        VrButton {
            width: 110
            text: "▲  Up"
            onClicked: panel.session.up()
        }
        VrButton {
            width: 64
            text: "◀"
            enabled: panel.page > 0
            onClicked: panel.page--
        }
        Text {
            width: pager.parent.width - 110 - 2 * 64 - 3 * pager.spacing
            height: Style.buttonHeight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Page " + (panel.page + 1) + " / " + panel.pageCount
            color: Style.textDim
            font.pixelSize: Style.fontBody
        }
        VrButton {
            width: 64
            text: "▶"
            enabled: panel.page < panel.pageCount - 1
            onClicked: panel.page++
        }
    }
}
