// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Card shared by all headset panels (Claude Generated 2026): title row (title on the left, status or
// a button on the right), a separator, then the panel's own rows. The height follows the content.
import QtQuick
import "VrStyle.js" as Style

Rectangle {
    id: panel

    property string title
    /// Items placed at the right end of the title row (a state chip, a close button).
    property alias headerItems: headerRow.data
    default property alias content: body.data

    width: 300
    height: column.implicitHeight + 2 * Style.padding
    radius: Style.radius
    color: Style.panelColor
    border.color: Style.panelBorder
    border.width: 2

    Column {
        id: column
        x: Style.padding
        y: Style.padding
        width: panel.width - 2 * Style.padding
        spacing: Style.spacing

        Item {
            width: parent.width
            height: Math.max(Style.headerHeight, headerRow.implicitHeight)
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: panel.title
                color: Style.title
                font.pixelSize: Style.fontTitle
                font.weight: Font.DemiBold
            }
            Row {
                id: headerRow
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Style.spacing
            }
        }
        Rectangle {
            width: parent.width
            height: 2
            color: Style.separator
        }
        Column {
            id: body
            width: parent.width
            spacing: Style.spacing
        }
    }
}
