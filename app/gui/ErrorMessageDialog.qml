import QtQuick 2.15
import QtQuick.Controls 2.2

import SystemProperties 1.0

NavigableMessageDialog {
    standardButtons: Dialog.Ok
                     | (SystemProperties.hasBrowser && helpUrl.length > 0
                        ? Dialog.Help : 0)
    okText: qsTr("Done")
}
