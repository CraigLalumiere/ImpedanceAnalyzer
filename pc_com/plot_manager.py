from PySide6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg
import numpy as np
from messages.AddXYToPlot_pb2 import AddXYToPlot
from messages.ClearPlots_pb2 import ClearPlots
from messages.DrawBodePlot_pb2 import DrawBodePlot
from messages.DrawPlot_pb2 import DrawPlot
from messages.LogToPlot_pb2 import LogToPlot
import random
import math




class PlotManager:
    def __init__(self, plot_window: pg.GraphicsLayoutWidget):
        self.plot_win = plot_window

        self.plots = []
        self.plot_data = []

        self.plot_win.setBackground(QtGui.QColor('#FFFFFF'))

        
        self.colours = ['b', 'g', 'r', 'c', 'm', 'y', 'k']


    def update_data(self, msg):
        plot_number = msg.plot_number

        # check if this plot exists
        while plot_number+1 > len(self.plots):
            self.add_new_plot()

        data_label = msg.data_label
        # check if we've not seen this label before
        if not data_label in self.plot_data[plot_number]:
            if (len(self.plot_data[plot_number]) >= len(self.colours)):
                self.colours.append((random.random()*256, random.random()*256, random.random()*256))
            thisColour = self.colours[len(self.plot_data[plot_number])]
            thisCurve = self.plots[plot_number].plot([],
                                                    [],
                                                    pen=thisColour,
                                                    name=data_label)
            self.plot_data[plot_number][data_label] = {'x':[], 'y':[], 'curve':thisCurve} # empty

        if isinstance(msg, LogToPlot):
            if len(self.plot_data[plot_number][data_label]['x']) < 50:
                self.plot_data[plot_number][data_label]['x'].append(msg.milliseconds_tick/1000)
                self.plot_data[plot_number][data_label]['y'].append(msg.data_y)
            else:
                self.plot_data[plot_number][data_label]['x'][:-1] = self.plot_data[plot_number][data_label]['x'][1:]
                self.plot_data[plot_number][data_label]['x'][-1] = msg.milliseconds_tick/1000
                self.plot_data[plot_number][data_label]['y'][:-1] = self.plot_data[plot_number][data_label]['y'][1:]
                self.plot_data[plot_number][data_label]['y'][-1] = msg.data_y

        if isinstance(msg, AddXYToPlot):
            if len(self.plot_data[plot_number][data_label]['x']) < 50:
                self.plot_data[plot_number][data_label]['x'].append(msg.data_x)
                self.plot_data[plot_number][data_label]['y'].append(msg.data_y)
            else:
                self.plot_data[plot_number][data_label]['x'][:-1] = self.plot_data[plot_number][data_label]['x'][1:]
                self.plot_data[plot_number][data_label]['x'][-1] = msg.data_x
                self.plot_data[plot_number][data_label]['y'][:-1] = self.plot_data[plot_number][data_label]['y'][1:]
                self.plot_data[plot_number][data_label]['y'][-1] = msg.data_y
        
        if isinstance(msg, DrawPlot):
            self.plot_data[plot_number][data_label]['x'] = msg.data_x._values
            self.plot_data[plot_number][data_label]['y'] = msg.data_y._values

    def draw_bode_plot(self, msg):
        plot_number = msg.plot_number

        # bode plot will draw magnitude on plot_number and phase on plot_number+1

        # ensure both of these plots exists
        while plot_number+2 > len(self.plots):
            self.add_new_plot()

        data_label = msg.data_label
        # add the magnitude plot if it doesnt already exist
        if not data_label in self.plot_data[plot_number]:
            if (len(self.plot_data[plot_number]) >= len(self.colours)):
                self.colours.append((random.random()*256, random.random()*256, random.random()*256))
            thisColour = self.colours[len(self.plot_data[plot_number])]
            thisCurve = self.plots[plot_number].plot([],
                                                    [],
                                                    pen=thisColour,
                                                    name='|'+data_label+'|')
            self.plot_data[plot_number][data_label] = {'x':[], 'y':[], 'curve':thisCurve} # empty
        
        # add the phase plot if it doesnt already exist
        if not data_label in self.plot_data[plot_number+1]:
            if (len(self.plot_data[plot_number+1]) >= len(self.colours)):
                self.colours.append((random.random()*256, random.random()*256, random.random()*256))
            thisColour = self.colours[len(self.plot_data[plot_number+1])]
            thisCurve = self.plots[plot_number+1].plot([],
                                                    [],
                                                    pen=thisColour,
                                                    name='∠'+data_label)
            self.plot_data[plot_number+1][data_label] = {'x':[], 'y':[], 'curve':thisCurve} # empty

        x = msg.data_freq._values
        mag = msg.data_mag._values
        self.plot_data[plot_number][data_label]['x'] = x
        self.plot_data[plot_number+1][data_label]['x'] = x
        self.plot_data[plot_number][data_label]['y'] = mag
        self.plot_data[plot_number+1][data_label]['y'] = msg.data_phase._values
        
        xRange = math.log10(math.sqrt(max(x)/min(x)))
        yCenter = math.log10(math.sqrt(max(mag)*min(mag)))
        self.plots[plot_number].setYRange(yCenter-xRange, yCenter+xRange, padding=0)
        self.plots[plot_number+1].setYRange(-180, 180, padding=0)
        self.plots[plot_number].setXRange(math.log10(min(x)), math.log10(max(x)), padding=0)
        self.plots[plot_number+1].setXRange(math.log10(min(x)), math.log10(max(x)), padding=0)


    def update_plot(self):
        for data in self.plot_data: # loop through each plot
            for data_label in data: # loop through each curve on this plot
                x = data[data_label]['x']
                y = data[data_label]['y']
                data[data_label]['curve'].setData(x, y)

    def add_new_plot(self):
        self.plots.append(self.plot_win.addPlot(row=len(self.plots), col=0)) # title="Title"
        self.plots[-1].addLegend()
        self.plots[-1].setLabel('bottom', 'Time', 'Hz')
        self.plots[-1].showGrid(x=True, y=True, alpha=0.5)
        if len(self.plots) == 1:
            self.plots[-1].setLogMode(True, True)
        elif len(self.plots) == 2:
            self.plots[-1].setLogMode(True, False)
        self.plot_data.append({}) # empty dictionary
    
    def reset_plots(self):
        for plot in self.plots:
            self.plot_win.removeItem(plot)
            plot.deleteLater()
        self.plots = []
        self.plot_data = []

