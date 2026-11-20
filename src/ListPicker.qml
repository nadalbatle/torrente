import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Picks one value from a short list, from the keyboard or the mouse.
Sheet {
    id: picker
    // Each {label, value}.
    property var options: []
    property int currentIndex: 0
    signal chosen(var value)

    function open(initialValue) {
        currentIndex = 0
        for (var i = 0; i < options.length; i++)
            if (options[i].value === initialValue)
                currentIndex = i
        visible = true
        list.forceActiveFocus()
    }

    ListView {
        id: list
        width: parent.width
        height: Math.min(contentHeight, 320)
        clip: true
        model: picker.options
        currentIndex: picker.currentIndex
        onCurrentIndexChanged: picker.currentIndex = currentIndex
        keyNavigationWraps: true
        Keys.onReturnPressed: picker.chosen(picker.options[currentIndex].value)
        Keys.onEnterPressed: picker.chosen(picker.options[currentIndex].value)
        Keys.onPressed: (event) => {
            if (event.text === "j") { incrementCurrentIndex(); event.accepted = true }
            else if (event.text === "k") { decrementCurrentIndex(); event.accepted = true }
        }

        delegate: Item {
            required property var modelData
            required property int index
            width: ListView.view.width
            height: 34
            Rectangle {
                anchors.fill: parent
                radius: Math.min(4, theme.radius)
                color: index === list.currentIndex ? theme.selected : "transparent"
            }
            Rectangle {
                visible: index === list.currentIndex
                width: 3; height: 16; radius: 1.5
                anchors.verticalCenter: parent.verticalCenter
                color: theme.accent
            }
            Label {
                textFormat: Text.PlainText
                anchors.verticalCenter: parent.verticalCenter
                x: 14
                text: modelData.label
                color: index === list.currentIndex ? theme.foreground : theme.foreground
                font.pixelSize: 13
            }
            TapHandler {
                onTapped: {
                    list.currentIndex = index
                    picker.chosen(modelData.value)
                }
            }
        }
    }
}
