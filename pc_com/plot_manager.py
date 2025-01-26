from PySide6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg
import numpy as np
from messages.MotorDataLog_pb2 import MotorDataLog
from messages.Box1SensorDataLog_pb2 import Box1SensorDataLog



class PlotManager:
    def __init__(self, plot_window: pg.GraphicsLayoutWidget):
        self.plot_win = plot_window

        self.motor_data = [{"id": k,
                           "tick": np.zeros(1000),
                           "duty": np.zeros(1000),
                           "current_setpoint": np.zeros(1000),
                           "current": np.zeros(1000)}
                           for k in range(3)]

        self.box1_sensor_data = {
                           "tick": np.zeros(1000),
                           "pressure": np.zeros(1000),
                           "flow": np.zeros(1000)}

        self.plot_win.setBackground(QtGui.QColor('#FFFFFF'))

##############  Top plot ##############

        self.plot_current_1 = self.plot_win.addPlot(row=0, col=0, title="Motor Current (A)")
        self.plot_current_1.addLegend()
        self.plot_current_1_curve_motor_1 = self.plot_current_1.plot(self.motor_data[0]["tick"],
                                                                         self.motor_data[0]["current"],
                                                                         pen='b',
                                                                         name="big motor current")

        self.plot_current_1_curve_setpoint_1 = self.plot_current_1.plot(self.motor_data[0]["tick"],
                                                                         self.motor_data[0]["current_setpoint"],
                                                                         pen='r',
                                                                         name="big motor current setpoint")

        self.plot_current_1.setLabel('bottom', 'Time', 's')
        self.plot_current_1.showGrid(x=True, y=True, alpha=0.5)

##############  Bottom plot ##############

        self.plot_2 = self.plot_win.addPlot(row=1, col=0, title="Pressure (PSI), Flow (LPM))")
        self.plot_2.addLegend()
        self.plot_2_curve_pressure = self.plot_2.plot(self.box1_sensor_data["tick"],
                                                                         self.box1_sensor_data["pressure"],
                                                                         pen='b',
                                                                         name="box 1 pressure")
        self.plot_2_curve_flow = self.plot_2.plot(self.box1_sensor_data["tick"],
                                                                         self.box1_sensor_data["flow"],
                                                                         pen='r',
                                                                         name="box 1 flow")

        self.plot_2.setLabel('bottom', 'Time', 's')
        self.plot_2.showGrid(x=True, y=True, alpha=0.5)


    def update_data(self, msg):
        if isinstance(msg, MotorDataLog):
            self.motor_data[msg.motor_id]["tick"][:-1] = self.motor_data[msg.motor_id]["tick"][1:]
            self.motor_data[msg.motor_id]["tick"][-1] = msg.milliseconds_tick / 1000.0
            self.motor_data[msg.motor_id]["current"][:-1] = self.motor_data[msg.motor_id]["current"][1:]
            self.motor_data[msg.motor_id]["current"][-1] = msg.motor_current
            self.motor_data[msg.motor_id]["current_setpoint"][:-1] = self.motor_data[msg.motor_id]["current_setpoint"][1:]
            self.motor_data[msg.motor_id]["current_setpoint"][-1] = msg.motor_current_setpoint
        if isinstance(msg, Box1SensorDataLog):
            self.box1_sensor_data["tick"][:-1] = self.box1_sensor_data["tick"][1:]
            self.box1_sensor_data["tick"][-1] = msg.milliseconds_tick / 1000.0
            self.box1_sensor_data["pressure"][:-1] = self.box1_sensor_data["pressure"][1:]
            self.box1_sensor_data["pressure"][-1] = msg.pressure_PSI
            self.box1_sensor_data["flow"][:-1] = self.box1_sensor_data["flow"][1:]
            self.box1_sensor_data["flow"][-1] = msg.flow_LPM


    def update_plot(self):
        x_data = self.motor_data[0]["tick"]
        x_data = x_data - x_data[-1]
        self.plot_current_1_curve_motor_1.setData(x_data, self.motor_data[0]["current"])
        self.plot_current_1_curve_setpoint_1.setData(x_data, self.motor_data[0]["current_setpoint"])
        self.plot_current_1.setXRange(-60, 0, padding=0)
        # if np.any(x_data):
        #     xlim_min = np.ndarray.min(x_data[np.nonzero(x_data)])
        #     xlim_max = np.ndarray.max(x_data)
        #     self.plot_current_1.setXRange(xlim_min, xlim_max, padding=0)

        x_data = self.box1_sensor_data["tick"]
        x_data = x_data - x_data[-1]
        self.plot_2_curve_pressure.setData(x_data, self.box1_sensor_data["pressure"])
        self.plot_2_curve_flow.setData(x_data, self.box1_sensor_data["flow"])
        self.plot_2.setXRange(-60, 0, padding=0)
        # if np.any(x_data):
        #     xlim_min = np.ndarray.min(x_data[np.nonzero(x_data)])
        #     xlim_max = np.ndarray.max(x_data)
        #     self.plot_2.setXRange(xlim_min, xlim_max, padding=0)


