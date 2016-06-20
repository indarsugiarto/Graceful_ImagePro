import random
import struct
from PyQt4 import QtGui, QtCore

class imgWidget(QtGui.QWidget):
    def __init__(self, Title, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.setWindowTitle(Title)
        self.viewPort = QtGui.QGraphicsView(self)
        self.scene = QtGui.QGraphicsScene(self)
        self.viewPort.setScene(self.scene)

        vLayout = QtGui.QVBoxLayout(self)
        vLayout.addWidget(self.viewPort)

        self.setLayout(vLayout)

    def setPixmap(self, img):
        """
        call this function to draw pixmap
        """
        self.scene.clear()
        self.scene.addPixmap(img)
        self.viewPort.update()

class trxIndicator(QtGui.QWidget):
    def __init__(self, parent=None):
        QtGui.QWidget.__init__(self, parent)
        self.label = QtGui.QLabel(self)
        self.bar = QtGui.QProgressBar(self)
        self.bar.setMinimum(0)
        self.bar.setMaximum(100)
        self.bar.setTextVisible(True)
        self.maxVal = 0
        vLayout = QtGui.QVBoxLayout(self)
        vLayout.addWidget(self.label)
        vLayout.addWidget(self.bar)
        self.setLayout(vLayout)

        w = 400
        h = 50
        app = QtGui.QApplication
        rec = app.desktop()
        hScreen = rec.height()
        wScreen = rec.width()
        x = (wScreen - w) / 2
        y = (hScreen - h) / 2
        self.setGeometry(x,y,w,h)

    def setName(self, Name):
        self.label.setText(Name)

    def setMaxVal(self, val):
        self.maxVal = val

    def setVal(self, val):
        v = val * 100 / self.maxVal
        self.bar.setValue(v)

    def appendVal(self, val):
        self.bar.setValue(self.bar.value() + val)
