"""
The idea is:
- Use QImage for direct pixel manipulation and use QPixmap for rendering
- http://www.doc.gold.ac.uk/~mas02fl/MSC101/ImageProcess/edge.html
"""
from PyQt4 import Qt, QtGui, QtCore, QtNetwork
import mainGUI


IMG_R_BUFF0_BASE = 0x61000000
IMG_G_BUFF0_BASE = 0x62000000
IMG_B_BUFF0_BASE = 0x63000000
DEF_HOST = '192.168.240.253'
DEF_SEND_PORT = 17893 #tidak bisa diganti dengan yang lain
DEF_REPLY_PORT = 20000
chipX = 0
chipY = 0
sdpImgRedPort = 1
sdpImgGreenPort = 2
sdpImgBluePort = 3
sdpImgConfigPort = 7
SDP_CMD_CONFIG = 1
SDP_CMD_PROCESS = 2
SDP_CMD_CLEAR = 3
IMG_OP_SOBEL_NO_FILT	= 1	# will be carried in arg2.low
IMG_OP_SOBEL_WITH_FILT	= 2	# will be carried in arg2.low
IMG_OP_LAP_NO_FILT		= 3	# will be carried in arg2.low
IMG_OP_LAP_WITH_FILT	= 4	# will be carried in arg2.low
IMG_NO_FILTERING		= 0
IMG_WITH_FILTERING		= 1
IMG_SOBEL				= 0
IMG_LAPLACE				= 1

sdpCore = 1

class imgdecGUI(QtGui.QWidget, mainGUI.Ui_pyImgDec):
    def __init__(self, def_image_dir = "./", parent=None):
        self.img_dir = def_image_dir
        self.img = None
        super(imgdecGUI, self).__init__(parent)
        self.setupUi(self)
        self.connect(self.pbLoadSend, QtCore.SIGNAL("clicked()"), QtCore.SLOT("pbLoadSendClicked()"))


    @QtCore.pyqtSlot()
    def pbLoadSendClicked(self):
        self.fName = QtGui.QFileDialog.getOpenFileName(parent=self, caption="Load image file", \
                                                       directory = self.img_dir, filter = "*.*")
        #print self.fName    #this will contains the full path as well!
        if self.fName == "" or self.fName is None:
            return

    def sendSDP(self,flags, tag, dp, dc, dax, day, cmd, seq, arg1, arg2, arg3, bArray):
        """
        The detail experiment with sendSDP() see mySDPinger.py
        """
        da = (dax << 8) + day
        dpc = (dp << 5) + dc
        sa = 0
        spc = 255
        pad = struct.pack('2B',0,0)
        hdr = struct.pack('4B2H',flags,tag,dpc,spc,da,sa)
        scp = struct.pack('2H3I',cmd,seq,arg1,arg2,arg3)
        if bArray is not None:
            sdp = pad + hdr + scp + bArray
        else:
            sdp = pad + hdr + scp

        CmdSock = QtNetwork.QUdpSocket()
        CmdSock.writeDatagram(sdp, QtNetwork.QHostAddress(DEF_HOST), DEF_SEND_PORT)
        return sdp


"""
To read the size of file:
coba = os.stat('nama_file')
print coba.st_size
"""