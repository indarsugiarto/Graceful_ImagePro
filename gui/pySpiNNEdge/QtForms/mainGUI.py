# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'mainGUI.ui'
#
# Created: Tue Jun 21 10:05:47 2016
#      by: PyQt4 UI code generator 4.10.2
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

try:
    _fromUtf8 = QtCore.QString.fromUtf8
except AttributeError:
    def _fromUtf8(s):
        return s

try:
    _encoding = QtGui.QApplication.UnicodeUTF8
    def _translate(context, text, disambig):
        return QtGui.QApplication.translate(context, text, disambig, _encoding)
except AttributeError:
    def _translate(context, text, disambig):
        return QtGui.QApplication.translate(context, text, disambig)

class Ui_pySpiNNEdge(object):
    def setupUi(self, pySpiNNEdge):
        pySpiNNEdge.setObjectName(_fromUtf8("pySpiNNEdge"))
        pySpiNNEdge.resize(300, 300)
        pySpiNNEdge.setMinimumSize(QtCore.QSize(300, 300))
        pySpiNNEdge.setMaximumSize(QtCore.QSize(300, 300))
        self.pbLoad = QtGui.QPushButton(pySpiNNEdge)
        self.pbLoad.setGeometry(QtCore.QRect(10, 160, 81, 29))
        self.pbLoad.setObjectName(_fromUtf8("pbLoad"))
        self.frame = QtGui.QFrame(pySpiNNEdge)
        self.frame.setGeometry(QtCore.QRect(10, 50, 281, 91))
        self.frame.setFrameShape(QtGui.QFrame.StyledPanel)
        self.frame.setFrameShadow(QtGui.QFrame.Raised)
        self.frame.setObjectName(_fromUtf8("frame"))
        self.cbFilter = QtGui.QCheckBox(self.frame)
        self.cbFilter.setGeometry(QtCore.QRect(10, 60, 171, 26))
        self.cbFilter.setObjectName(_fromUtf8("cbFilter"))
        self.label = QtGui.QLabel(self.frame)
        self.label.setGeometry(QtCore.QRect(20, 0, 65, 21))
        self.label.setObjectName(_fromUtf8("label"))
        self.cbMode = QtGui.QComboBox(self.frame)
        self.cbMode.setGeometry(QtCore.QRect(20, 20, 131, 31))
        self.cbMode.setObjectName(_fromUtf8("cbMode"))
        self.cbMode.addItem(_fromUtf8(""))
        self.cbMode.addItem(_fromUtf8(""))
        self.pbClear = QtGui.QPushButton(pySpiNNEdge)
        self.pbClear.setGeometry(QtCore.QRect(210, 160, 81, 29))
        self.pbClear.setObjectName(_fromUtf8("pbClear"))
        self.frame_2 = QtGui.QFrame(pySpiNNEdge)
        self.frame_2.setGeometry(QtCore.QRect(10, 210, 281, 71))
        self.frame_2.setFrameShape(QtGui.QFrame.StyledPanel)
        self.frame_2.setFrameShadow(QtGui.QFrame.Raised)
        self.frame_2.setObjectName(_fromUtf8("frame_2"))
        self.label_2 = QtGui.QLabel(self.frame_2)
        self.label_2.setGeometry(QtCore.QRect(20, 0, 91, 21))
        self.label_2.setObjectName(_fromUtf8("label_2"))
        self.pbProcess = QtGui.QPushButton(self.frame_2)
        self.pbProcess.setGeometry(QtCore.QRect(20, 30, 101, 29))
        self.pbProcess.setObjectName(_fromUtf8("pbProcess"))
        self.pbSave = QtGui.QPushButton(self.frame_2)
        self.pbSave.setGeometry(QtCore.QRect(150, 30, 101, 29))
        self.pbSave.setObjectName(_fromUtf8("pbSave"))
        self.pbSend = QtGui.QPushButton(pySpiNNEdge)
        self.pbSend.setGeometry(QtCore.QRect(110, 160, 81, 29))
        self.pbSend.setObjectName(_fromUtf8("pbSend"))
        self.cbSpiNN = QtGui.QComboBox(pySpiNNEdge)
        self.cbSpiNN.setGeometry(QtCore.QRect(150, 10, 141, 31))
        self.cbSpiNN.setObjectName(_fromUtf8("cbSpiNN"))
        self.cbSpiNN.addItem(_fromUtf8(""))
        self.cbSpiNN.addItem(_fromUtf8(""))
        self.label_3 = QtGui.QLabel(pySpiNNEdge)
        self.label_3.setGeometry(QtCore.QRect(23, 14, 121, 21))
        self.label_3.setObjectName(_fromUtf8("label_3"))

        self.retranslateUi(pySpiNNEdge)
        QtCore.QMetaObject.connectSlotsByName(pySpiNNEdge)

    def retranslateUi(self, pySpiNNEdge):
        pySpiNNEdge.setWindowTitle(_translate("pySpiNNEdge", "Simple Edge Detection", None))
        self.pbLoad.setText(_translate("pySpiNNEdge", "Load Image", None))
        self.cbFilter.setText(_translate("pySpiNNEdge", "Filter Image", None))
        self.label.setText(_translate("pySpiNNEdge", "Mode", None))
        self.cbMode.setItemText(0, _translate("pySpiNNEdge", "Sobel", None))
        self.cbMode.setItemText(1, _translate("pySpiNNEdge", "Laplace", None))
        self.pbClear.setText(_translate("pySpiNNEdge", "Clear", None))
        self.label_2.setText(_translate("pySpiNNEdge", "For Python:", None))
        self.pbProcess.setText(_translate("pySpiNNEdge", "Process Image", None))
        self.pbSave.setText(_translate("pySpiNNEdge", "Save Result", None))
        self.pbSend.setText(_translate("pySpiNNEdge", "Send Image", None))
        self.cbSpiNN.setItemText(0, _translate("pySpiNNEdge", "SpiNN3", None))
        self.cbSpiNN.setItemText(1, _translate("pySpiNNEdge", "SpiNN5", None))
        self.label_3.setText(_translate("pySpiNNEdge", "SpiNNaker-board:", None))

