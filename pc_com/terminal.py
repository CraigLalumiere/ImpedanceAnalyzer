from PySide6 import QtWidgets, QtCore, QtGui
from PySide6.QtGui import QTextCursor

def qt_key_to_ascii(event: QtGui.QKeyEvent):
    if event.key() == QtCore.Qt.Key.Key_Return:
        return b"\r"
    elif event.key() == QtCore.Qt.Key.Key_Space:
        return b" "
    elif event.key() == QtCore.Qt.Key.Key_Enter:
        return b"\r"
    elif event.key() == QtCore.Qt.Key.Key_Tab:
        return b"\t"
    elif event.key() == QtCore.Qt.Key.Key_Backspace:
        return b"\b"
    elif event.key() == QtCore.Qt.Key.Key_Left:
        #return b"\x1b[D"
        return None # TODO: some issues with left and right movement. disabling for now.
    elif event.key() == QtCore.Qt.Key.Key_Right:
        #return b"\x1b[C"
        return None # TODO: some issues with left and right movement. disabling for now.
    elif event.key() == QtCore.Qt.Key.Key_Up:
        return b"\x1b[A"
    elif event.key() == QtCore.Qt.Key.Key_Down:
        return b"\x1b[B"
    elif event.text() in ('abcdefghijklmnopqrstuvwxyz'
                          'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
                          '[],=-.;/`&^~*@|#(){}$><%+?"_!'
                           "'\\"):
        return event.text().encode('utf8')

class Terminal(QtWidgets.QPlainTextEdit):
    key_pressed_evt = QtCore.Signal(bytes)

    # states
    (NORMAL,
     AFTER_ESC,
     AFTER_CSI) = range(3)


    def __init__(self, parent=None):
        super(Terminal, self).__init__()
        self.state = Terminal.NORMAL
        self.CSI_n = None
        self.current_line_start_index = 0
        self.last_char_received = None

        self.saved_cursor_pos = None
        self.cursor_position = 0

        self.setFont(QtGui.QFont("Courier", 12))
        fmt = QtGui.QFontMetrics(self.font())
        char_width = fmt.boundingRect("w").width()
        self.setCursorWidth(char_width)

    def reset(self):
        self.state = Terminal.NORMAL
        self.CSI_n = None
        self.current_line_start_index = 0
        self.last_char_received = None

        self.saved_cursor_pos = None
        self.cursor_position = 0

        self.setPlainText("")

    def dragEnterEvent(self, e):
        # disable ability to drag text around
        return

    def keyPressEvent(self, event: QtGui.QKeyEvent):
        code = qt_key_to_ascii(event)
        if code is not None:
            self.key_pressed_evt.emit(code)

    def mousePressEvent(self, event):
        cursor = self.textCursor()
        if cursor.selectedText():
            # restore cursor position
            cursor.setPosition(self.cursor_position)
            self.setTextCursor(cursor)

        return super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        cursor = self.textCursor()

        if event.button() == QtCore.Qt.MouseButton.LeftButton:
            if not cursor.selectedText():
                # restore cursor position
                cursor.setPosition(self.cursor_position)
                self.setTextCursor(cursor)

        return super().mouseReleaseEvent(event)

    def feed(self, data: bytes):
        data_str = data.decode("utf-8")

        # restore cursor position (if moved by mouse)
        cursor = self.textCursor()
        cursor.setPosition(self.cursor_position)
        self.setTextCursor(cursor)

        for c in data_str:
            if self.state == Terminal.NORMAL:
                # ANSI ESC
                if c == '\x1b':
                    self.state = Terminal.AFTER_ESC
                else:
                    cursor = self.textCursor()

                    if c == '\r':
                        cursor.setPosition(self.current_line_start_index)
                        self.setTextCursor(cursor)

                    elif c == '\n' and self.last_char_received == '\r':
                        # just returned to line home, so go forward to the end of the line
                        cursor.setPosition(len(self.toPlainText()))
                        self.setTextCursor(cursor)
                        cursor.insertText(c)
                        self.current_line_start_index = cursor.position()

                    else:
                        # replace character at cursor if one exists
                        if cursor.position() < len(self.toPlainText()):
                            cursor.deleteChar()

                        cursor.insertText(c)

            elif self.state == Terminal.AFTER_ESC:
                if c == '[':
                    self.state = Terminal.AFTER_CSI
                # don't handle non-CSI ESC commands
                else:
                    self.state = Terminal.NORMAL

            elif self.state == Terminal.AFTER_CSI:
                # handle CSI n parameter, remain in CSI state
                if c in '123456789':
                    self.CSI_n = int(c)
                else:
                    self.CSI_n = 1

                    # cursor forward (right)
                    if c == 'C':
                        self.state = Terminal.NORMAL
                        cursor = self.textCursor()
                        for n in range(self.CSI_n):
                            cursor.movePosition(QTextCursor.MoveOperation.Right)
                        self.setTextCursor(cursor)

                    # cursor backward (left)
                    # just need to handle single moves at a time
                    elif c == 'D':
                        self.state = Terminal.NORMAL
                        cursor = self.textCursor()
                        for n in range(self.CSI_n):
                            cursor.movePosition(QTextCursor.MoveOperation.Left)
                        self.setTextCursor(cursor)

                    # cursor save position
                    elif c == 's':
                        self.state = Terminal.NORMAL
                        cursor = self.textCursor()
                        self.saved_cursor_pos = cursor.position()

                    # cursor restore position
                    elif c == 'u':
                        self.state = Terminal.NORMAL
                        if self.saved_cursor_pos is not None:
                            cursor = self.textCursor()
                            cursor.setPosition(self.saved_cursor_pos)
                            self.setTextCursor(cursor)

                    # cursor insert character (ICH)
                    elif c == '@':
                        self.state = Terminal.NORMAL
                        cursor = self.textCursor()
                        cursor.insertText(" ")

                    # cursor delete character (DCH)
                    elif c == 'P':
                        self.state = Terminal.NORMAL
                        cursor = self.textCursor()
                        cursor.deleteChar()

                    # not handled, return to normal
                    else:
                        self.state = Terminal.NORMAL

            # save last char
            self.last_char_received = c

        # save new cursor position (so that it can be restored if changed by mouse)
        cursor = self.textCursor()
        self.cursor_position = cursor.position()


if __name__ == "__main__":
    import sys

    # Create the Qt application and console.
    app = QtWidgets.QApplication([])
    mainwin = QtWidgets.QMainWindow()
    title = "terminal"
    mainwin.setWindowTitle(title)

    console = Terminal(mainwin)
    mainwin.setCentralWidget(console)

    console.feed(b't\x1b[sest\x1b[u')
    # console.feed(b'test\x1b[D\x1b[P')
    # for i in range(10):
    #     console.feed(b'test\n')
    #     console.feed(b'test\n')

    # Show widget and launch Qt's event loop.
    mainwin.show()
    sys.exit(app.exec())





