import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import GetThermal 1.0
import "qrc:/controls"
import "qrc:/images"

Item {
    id: item1
    anchors.fill: parent

    property alias acq: acq
    property alias player: player
    property alias videoOutput: videoOutput
    width: 640

    UvcAcquisition {
        id: acq
    }

    UvcVideoProducer {
        id: player
        uvc: acq
        videoSink: videoOutput.videoSink
    }

    RowLayout {
        spacing: 0
        anchors.fill: parent

        CameraControls {
            Layout.minimumWidth: 240
            Layout.fillHeight: true
            acq: acq
        }

        Pane {
            x: 220
            width: 400
            bottomPadding: 5
            rightPadding: 5
            leftPadding: 5
            topPadding: 5
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent

                VideoOutput {
                    id: videoOutput
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    fillMode: VideoOutput.PreserveAspectFit
                    VideoRoi {
                        id: radRoi
                        acq: acq
                        showMinMax: switchMinMax.checked
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Switch {
                    id: switchMinMax
                    text: qsTr("Show Min/Max Points")
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }




        Pane {
            width: 130
            Layout.minimumWidth: 130
            Layout.fillHeight: true
            visible: acq.cci.supportsRadiometry

            SpotInfo {
                id: spotInfo
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                acq: acq
                farenheitTemps: rangeDisplay.farenheitTemps
            }

            RangeDisplay {
                id: rangeDisplay
                anchors.top: spotInfo.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                acq: acq
            }
        }

    }

}
