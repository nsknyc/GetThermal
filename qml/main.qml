import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    visible: true
    width: 960
    height: 540
    title: qsTr("GetThermal")

    Viewer {
        anchors.fill: parent
    }
}
