from PySide6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg
import numpy as np
from messages.AddToPlot_pb2 import AddToPlot
from messages.DrawPlot_pb2 import DrawPlot



class PlotManager:
    def __init__(self, plot_window: pg.GraphicsLayoutWidget):
        self.plot_win = plot_window

        self.plots = []
        self.plot_data = []

        self.plot_win.setBackground(QtGui.QColor('#FFFFFF'))


    def update_data(self, msg):
        if isinstance(msg, AddToPlot) or isinstance(msg, DrawPlot):
            plot_number = msg.plot_number

            # check if this plot exists
            while plot_number+1 > len(self.plots):
                self.add_new_plot()

            data_label = msg.data_label
            # check if we've not seen this label before
            if not data_label in self.plot_data[plot_number]:
                colours = ['b', 'g', 'r', 'c', 'm', 'y', 'k']
                thisColour = colours[len(self.plot_data[plot_number])]
                thisCurve = self.plots[plot_number].plot([],
                                                        [],
                                                        pen=thisColour,
                                                        name=data_label)
                self.plot_data[plot_number][data_label] = {'x':[], 'y':[], 'curve':thisCurve} # empty

            if isinstance(msg, AddToPlot):
                if len(self.plot_data[plot_number][data_label]['x']) < 50:
                    self.plot_data[plot_number][data_label]['x'].append(msg.milliseconds_tick)
                    self.plot_data[plot_number][data_label]['y'].append(msg.data_point)
                else:
                    self.plot_data[plot_number][data_label]['x'][:-1] = self.plot_data[plot_number][data_label]['x'][1:]
                    self.plot_data[plot_number][data_label]['x'][-1] = msg.milliseconds_tick
                    self.plot_data[plot_number][data_label]['y'][:-1] = self.plot_data[plot_number][data_label]['y'][1:]
                    self.plot_data[plot_number][data_label]['y'][-1] = msg.data_point
            
            if isinstance(msg, DrawPlot):
                num_points = len(msg.data_points)
                self.plot_data[plot_number][data_label]['x'] = np.linspace(0, num_points - 1, num_points)
                self.plot_data[plot_number][data_label]['y'] = msg.data_points._values


    def update_plot(self):
        for data in self.plot_data: # loop through each plot
            for data_label in data: # loop through each curve on this plot
                x = data[data_label]['x']
                y = data[data_label]['y']
                data[data_label]['curve'].setData(x, y)

    def add_new_plot(self):
        self.plots.append(self.plot_win.addPlot(row=len(self.plots), col=0)) # title="Title"
        self.plots[-1].addLegend()
        self.plots[-1].setLabel('bottom', 'Time', 's')
        self.plots[-1].showGrid(x=True, y=True, alpha=0.5)
        self.plot_data.append({}) # empty dictionary
    
    def reset_plots(self):
        for plot in self.plots:
            self.plot_win.removeItem(plot)
            plot.deleteLater()
        self.plots = []
        self.plot_data = []

