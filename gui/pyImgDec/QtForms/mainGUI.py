# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'mainGUI.ui'
#
# Created: Mon Jun  6 12:02:38 2016
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

class Ui_pyImgDec(object):
    def setupUi(self, pyImgDec):
        pyImgDec.setObjectName(_fromUtf8("pyImgDec"))
        pyImgDec.resize(400, 300)
        self.pbLoadSend = QtGui.QPushButton(pyImgDec)
        self.pbLoadSend.setGeometry(QtCore.QRect(270, 20, 92, 29))
        self.pbLoadSend.setObjectName(_fromUtf8("pbLoadSend"))

        self.retranslateUi(pyImgDec)
        QtCore.QMetaObject.connectSlotsByName(pyImgDec)

    def retranslateUi(self, pyImgDec):
        pyImgDec.setWindowTitle(_translate("pyImgDec", "Image Decoder", None))
        self.pbLoadSend.setText(_translate("pyImgDec", "Load & Send", None))

