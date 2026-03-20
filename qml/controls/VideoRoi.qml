import QtQuick
import QtQuick.Controls
import GetThermal 1.0

Item {
    id: root

    anchors.fill: parent

    property UvcAcquisition acq: null
    property rect roi: acq && acq.cci ? acq.cci.radSpotmeterRoi : Qt.rect(0, 0, 80, 60)
    property size videoSize: acq ? acq.videoSize : Qt.size(80, 60)
    property bool showMinMax: false

    function moveUnscaledRoiTo(dispX, dispY) {
        var nx = Math.floor(dispX * (videoSize.width / scaledvid.width))
        var ny = Math.floor(dispY * (videoSize.height / scaledvid.height))

        // Click same spot again: reset to center
        if (roi.x === nx && roi.y === ny) {
            var cx = Math.floor((videoSize.width - roi.width) / 2)
            var cy = Math.floor((videoSize.height - roi.height) / 2)
            acq.cci.radSpotmeterRoi = Qt.rect(cx, cy, roi.width, roi.height)
            return
        }
        acq.cci.radSpotmeterRoi = Qt.rect(nx, ny, roi.width, roi.height)
    }

    Item {
        id: scaledvid

        width: root.width
        height: (root.width * videoSize.height) / videoSize.width
        anchors.centerIn: root

        MouseArea {
            cursorShape: Qt.CrossCursor
            anchors.fill: parent
            onClicked: (mouse) => moveUnscaledRoiTo(mouse.x, mouse.y)
        }

        Rectangle {
            id: roidisp

            x: roi.x * (scaledvid.width / videoSize.width)
            y: roi.y * (scaledvid.height / videoSize.height)
            width: roi.width * (scaledvid.width / videoSize.width)
            height: roi.height * (scaledvid.height / videoSize.height)

            color: "#80ffffff"
            border.color: "#80000000"

            MouseArea {
                cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                anchors.fill: parent
                drag.target: roidisp
                drag.axis: Drag.XAndYAxis
                drag.smoothed: false
                drag.threshold: 1
                onPositionChanged: {
                    moveUnscaledRoiTo(roidisp.x, roidisp.y)
                }
            }
        }

        // Max temperature point (red)
        Rectangle {
            id: maxpointMarker
            x: acq.dataFormatter.maxPoint.x * (scaledvid.width / videoSize.width) - width / 2
            y: acq.dataFormatter.maxPoint.y * (scaledvid.height / videoSize.height) - height / 2
            width: 8
            height: 8
            radius: 4
            color: "#ffff0000"
            border.color: "#ffffffff"
            border.width: 1
            visible: root.showMinMax

            Text {
                anchors.left: parent.right
                anchors.leftMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                text: "H"
                color: "#ffff0000"
                font.pixelSize: 10
                font.bold: true
                visible: parent.visible
            }
        }

        // Min temperature point (blue)
        Rectangle {
            id: minpointMarker
            x: acq.dataFormatter.minPoint.x * (scaledvid.width / videoSize.width) - width / 2
            y: acq.dataFormatter.minPoint.y * (scaledvid.height / videoSize.height) - height / 2
            width: 8
            height: 8
            radius: 4
            color: "#ff0080ff"
            border.color: "#ffffffff"
            border.width: 1
            visible: root.showMinMax

            Text {
                anchors.left: parent.right
                anchors.leftMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                text: "L"
                color: "#ff0080ff"
                font.pixelSize: 10
                font.bold: true
                visible: parent.visible
            }
        }
    }
}
