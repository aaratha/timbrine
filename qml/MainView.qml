import QtQuick 6.4
import QtQuick.Controls 6.4
import MyApp 1.0

ApplicationWindow {
    id: root
    width: 640
    height: 480
    visible: true
    color: "black"

    property int rectCount: Math.max(1, ui ? ui.binCount : 1)
    property int minRectWidth: 2
    property int activeIndex: -1
    property bool showDots: false
    property bool showVoronoi: true

    Connections {
        target: ui
        function onBinCountChanged() {
            if (root.activeIndex < 0 && ui && ui.binCount > 0) {
                root.activeIndex = 0
                ui.rectangleClicked(0)
            }
        }
    }

    Item {
        id: binStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        height: 80

        Row {
            id: row
            anchors.fill: parent
            spacing: 2

            property real cellWidth: Math.max(minRectWidth, (row.width - (rectCount - 1) * row.spacing) / rectCount)

            Repeater {
                model: rectCount
                delegate: Rectangle {
                    width: row.cellWidth
                    height: row.height
                    color: index === activeIndex ? "#f39c12" : "#e74c3c"
                    radius: 6
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            function updateIndex(xPos) {
                var cell = row.cellWidth + row.spacing
                var idx = Math.floor(xPos / cell)
                if (idx < 0 || idx >= rectCount) {
                    return
                }
                if (activeIndex !== idx) {
                    activeIndex = idx
                    ui.rectangleClicked(idx)
                }
            }

            onPressed: function(mouse) { updateIndex(mouse.x) }
            onPositionChanged: function(mouse) { if (pressed) updateIndex(mouse.x) }
        }
    }

    Rectangle {
        id: plotArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: binStrip.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 16
        color: "#101418"
        border.color: "#2c3e50"
        radius: 8

        VoronoiEdges {
            anchors.fill: parent
            edges: ui ? ui.voronoiEdges : []
            cells: ui ? ui.voronoiCells : []
            activeIndex: root.activeIndex
            visible: showVoronoi
        }

        Repeater {
            model: showDots && ui ? ui.umapPoints : []
            delegate: Item {
                width: 6
                height: 6

                readonly property real xNorm: modelData.x
                readonly property real yNorm: modelData.y

                x: xNorm * plotArea.width - width / 2
                y: (1 - yNorm) * plotArea.height - height / 2

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: modelData.index === activeIndex ? "#f39c12" : "#1abc9c"
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: false

            function updateIndex(xPos, yPos) {
                if (!ui) {
                    return
                }
                var w = plotArea.width
                var h = plotArea.height
                if (w <= 0 || h <= 0) {
                    return
                }
                var nx = xPos / w
                var ny = 1 - (yPos / h)
                var bestIndex = ui.cellIndexAt(nx, ny)
                if (bestIndex >= 0 && activeIndex !== bestIndex) {
                    activeIndex = bestIndex
                    ui.rectangleClicked(bestIndex)
                }
            }

            onPressed: function(mouse) { updateIndex(mouse.x, mouse.y) }
            onPositionChanged: function(mouse) { updateIndex(mouse.x, mouse.y) }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        spacing: 12

        CheckBox {
            text: "Voronoi"
            checked: showVoronoi
            onToggled: showVoronoi = checked
        }

        CheckBox {
            text: "Dots"
            checked: showDots
            onToggled: showDots = checked
        }
    }
}
