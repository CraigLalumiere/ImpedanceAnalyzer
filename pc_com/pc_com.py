import sys
import os
from PySide6 import QtWidgets
from PySide6.QtWidgets import QFileDialog, QInputDialog
from plot_manager import PlotManager
from PySide6 import QtCore
from messages.CLIData_pb2 import CLIData
from messages.AddToPlot_pb2 import AddToPlot
from messages.LogPrint_pb2 import LogPrint
from messages.DrawPlot_pb2 import DrawPlot

import packets
from main_window import Ui_MainWindow
from com_controller import ComController, get_com_ports
# from com_controller_fake import ComControllerFake

class ApplicationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super(ApplicationWindow, self).__init__()

        self.ui = Ui_MainWindow()
        self.ui.setupUi(self)

        self.controller = ComController()
        # self.controller = ComControllerFake()
        self.recording = False
        self.outfile = None
        self.outfile_errors = None

        self.data_folder = os.path.dirname(os.path.normpath(__file__))
        self.ui.txt_data_dir.setText(self.data_folder)

        # set app title
        self.setWindowTitle("NUCLEO-G474 Com Tool")

        self.update_interface_state()

        self.gui_refresh_interval = 50
        self.gui_refresh_timer = QtCore.QTimer()
        self.gui_refresh_timer.timeout.connect(self.on_gui_refresh_timer)
        self.gui_refresh_timer.start(self.gui_refresh_interval)

        self.console_test_timer = QtCore.QTimer()
        self.console_test_timer.timeout.connect(self.on_console_test_timer)
        # self.console_test_timer.start(3000)
        self.console_test_int = 64

        self.plot_manager = PlotManager(self.ui.plot)

        self._setup_ui_signals()

    def on_console_test_timer(self):
        self.ui.terminal.feed(self.console_test_int.to_bytes())
        self.console_test_int += 1

    def _setup_ui_signals(self):
        self.ui.btn_browse_dir.clicked.connect(self.on_btn_browse_clicked)
        self.ui.btn_port.clicked.connect(self.on_btn_port_clicked)
        self.ui.btn_connect.clicked.connect(self.on_btn_connect_clicked)
        self.ui.btn_disconnect.clicked.connect(self.on_btn_disconnect_clicked)
        self.ui.btn_start_record.clicked.connect(self.on_btn_start_record_clicked)
        self.ui.btn_stop_record.clicked.connect(self.on_btn_stop_record_clicked)
        self.ui.terminal.key_pressed_evt.connect(self.on_terminal_key_pressed)

    def on_gui_refresh_timer(self):
        evts = self.controller.update_and_get_events()
        for e in evts:
            self.handle_controller_event(e)

        msgs = self.controller.get_received_messages()
        for msg in msgs:
            # print(type(msg))
            self.handle_controller_msg_received(msg)

        # redraw plots
        self.plot_manager.update_plot()

    def handle_controller_msg_received(self, message):
        if isinstance(message, CLIData):
            print("Receive CLI data: {0}".format(message.msg))
            self.ui.terminal.feed(message.msg)

        if isinstance(message, LogPrint):
            msg_string = "{0} {1}".format(message.milliseconds_tick, message.msg)
            self.ui.txt_log.append(msg_string)

            if self.recording:
                self.outfile.write(msg_string + '\n')

        # for plots
        if isinstance(message, AddToPlot) or isinstance(message, DrawPlot):
            self.plot_manager.update_data(message)

        # todo record data used in plots

    def handle_controller_event(self, evt):
        if evt['event'] == 'error':
            if self.recording:
                self.recording = False

                if self.outfile is not None:
                    self.outfile.close()

                if self.outfile_errors is not None:
                    self.outfile_errors.close()
        elif evt['event'] == 'connection_status_changed':
            self.plot_manager.reset_plots()

            self.update_interface_state()      

    def on_btn_browse_clicked(self):
        folder = QFileDialog.getExistingDirectory(None,
                                                  'Select Data Directory',
                                                  self.data_folder)
        if folder != "":
            self.data_folder = os.path.normpath(folder)
            self.ui.txt_data_dir.setText(self.data_folder)

    def on_btn_port_clicked(self):
        com_ports = get_com_ports()
        port, ok = QInputDialog.getItem(self, "Select a port", "Serial port:", com_ports)
        if ok and port:
            self.ui.txt_port.setText(port)

        self.update_interface_state()

    def on_btn_connect_clicked(self):
        self.controller.connect(self.ui.txt_port.text())
        self.ui.txt_log.setText("")
        self.ui.terminal.reset()
        self.update_interface_state() 

    def on_btn_disconnect_clicked(self):
        self.controller.disconnect()

        self.recording = False

        if self.outfile is not None:
            self.outfile.close()

        if self.outfile_errors is not None:
            self.outfile_errors.close()

        self.update_interface_state()

    def on_btn_start_record_clicked(self):
        self.outfile = open(os.path.join(self.ui.txt_data_dir.text(), "log.txt"), 'w')
        self.outfile_errors = open(os.path.join(self.ui.txt_data_dir.text(), "log_errors.txt"), 'w')
        self.recording = True
        self.update_interface_state()

    def on_btn_stop_record_clicked(self):
        self.recording = False

        if self.outfile is not None:
            self.outfile.close()

        if self.outfile_errors is not None:
            self.outfile_errors.close()

        self.update_interface_state()

    def on_terminal_key_pressed(self, data):
        if self.controller.connected:
            self.controller.transmit_cli_data(data)

    def update_interface_state(self):
        if self.ui.txt_port.text() == '':
            self.ui.txt_status.setText("Disconnected")
            self.ui.txt_status.setStyleSheet("QLineEdit{background: rgb(237, 125, 49);}")
            self.ui.btn_connect.setEnabled(False)
            self.ui.btn_disconnect.setEnabled(False)
            self.ui.btn_start_record.setEnabled(False)
            self.ui.btn_stop_record.setEnabled(False)
        else:
            if self.controller.connected:
                if self.recording:
                    self.ui.txt_status.setText("Recording")
                    self.ui.txt_status.setStyleSheet("QLineEdit{background: rgb(237, 125, 49);}")
                    self.ui.btn_start_record.setEnabled(False)
                    self.ui.btn_stop_record.setEnabled(True)
                else:
                    self.ui.txt_status.setText("Connected")
                    self.ui.txt_status.setStyleSheet("QLineEdit{background: rgb(146, 208, 80);}")
                    self.ui.btn_connect.setEnabled(False)
                    self.ui.btn_disconnect.setEnabled(True)
                    self.ui.btn_start_record.setEnabled(True)
                    self.ui.btn_stop_record.setEnabled(False)
            else:
                self.ui.txt_status.setText("Disconnected")
                self.ui.txt_status.setStyleSheet("QLineEdit{background: rgb(237, 125, 49);}")
                self.ui.btn_connect.setEnabled(True) 
                self.ui.btn_disconnect.setEnabled(False)                
                self.ui.btn_start_record.setEnabled(False)
                self.ui.btn_stop_record.setEnabled(False)

    def closeEvent(self, event):
        """
        Application closed. Close gracefully.
        """
        self.controller.disconnect()

        if self.recording and self.outfile is not None:
            self.outfile.close()

        if self.recording and self.outfile_errors is not None:
            self.outfile_errors.close()


        event.accept()


def main():
    app = QtWidgets.QApplication(sys.argv)
    application = ApplicationWindow()
    application.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
