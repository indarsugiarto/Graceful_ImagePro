"""
The idea is:
- Use QImage for direct pixel manipulation and use QPixmap for rendering
- http://www.doc.gold.ac.uk/~mas02fl/MSC101/ImageProcess/edge.html
"""

UDP_BUFF_SIZE = 2048

from PyQt4 import Qt, QtGui, QtCore, QtNetwork
import mainGUI
import math
import os
import struct
import socket
import time

from imgViewer import imgWidget, trxIndicator

# Sobel operator
"""
 /* 3x3 GX Sobel mask.  Ref: www.cee.hw.ac.uk/hipr/html/sobel.html */
   GX[0][0] = -1; GX[0][1] = 0; GX[0][2] = 1;
   GX[1][0] = -2; GX[1][1] = 0; GX[1][2] = 2;
   GX[2][0] = -1; GX[2][1] = 0; GX[2][2] = 1;

   /* 3x3 GY Sobel mask.  Ref: www.cee.hw.ac.uk/hipr/html/sobel.html */
   GY[0][0] =  1; GY[0][1] =  2; GY[0][2] =  1;
   GY[1][0] =  0; GY[1][1] =  0; GY[1][2] =  0;
   GY[2][0] = -1; GY[2][1] = -2; GY[2][2] = -1;
"""
GX = [[-1,0,1],[-2,0,2],[-1,0,1]]
GY = [[1,2,1],[0,0,0],[-1,-2,-1]]

# Laplace operator
"""
   /* 5x5 Laplace mask.  Ref: Myler Handbook p. 135 */
   MASK[0][0] = -1; MASK[0][1] = -1; MASK[0][2] = -1; MASK[0][3] = -1; MASK[0][4] = -1;
   MASK[1][0] = -1; MASK[1][1] = -1; MASK[1][2] = -1; MASK[1][3] = -1; MASK[1][4] = -1;
   MASK[2][0] = -1; MASK[2][1] = -1; MASK[2][2] = 24; MASK[2][3] = -1; MASK[2][4] = -1;
   MASK[3][0] = -1; MASK[3][1] = -1; MASK[3][2] = -1; MASK[3][3] = -1; MASK[3][4] = -1;
   MASK[4][0] = -1; MASK[4][1] = -1; MASK[4][2] = -1; MASK[4][3] = -1; MASK[4][4] = -1;
"""
LAP = [[-1,-1,-1,-1,-1],
       [-1,-1,-1,-1,-1],
       [-1,-1,24,-1,-1],
       [-1,-1,-1,-1,-1],
       [-1,-1,-1,-1,-1]]

# Gaussian filter
FILT = [[2,4,5,4,2],
        [4,9,12,9,4],
        [5,12,15,12,5],
        [4,9,12,9,4],
        [2,4,5,4,2]]
FILT_DENOM = 159


IMG_R_BUFF0_BASE = 0x61000000
IMG_G_BUFF0_BASE = 0x62000000
IMG_B_BUFF0_BASE = 0x63000000


#----------------- SpiNNaker stuffs --------------------
CHIP_LIST_48 = [[0,0],[1,0],[2,0],[3,0],[4,0],\
                [0,1],[1,1],[2,1],[3,1],[4,1],[5,1],\
                [0,2],[1,2],[2,2],[3,2],[4,2],[5,2],[6,2],\
                [0,3],[1,3],[2,3],[3,3],[4,3],[5,3],[6,3],[7,3],\
                      [1,4],[2,4],[3,4],[4,4],[5,4],[6,4],[7,4],\
                            [2,5],[3,5],[4,5],[5,5],[6,5],[7,5],\
                                  [3,6],[4,6],[5,6],[6,6],[7,6],\
                                        [4,7],[5,7],[6,7],[7,7]]
"""
Today (21 June 11:47) I found that sdp chain is not working as expected. Some chips don't receive
the packets. Weird! So, let's reduce the size:
"""
CHIP_LIST_15 = [[0,0],[1,0],[2,0],[3,0],[4,0],
                [0,1],[1,1],[2,1],[3,1],[4,1],
                [0,2],[1,2],[2,2],[3,2],[4,2]]

CHIP_LIST_20 = [[0,0],[1,0],[2,0],[3,0],[4,0],
                [0,1],[1,1],[2,1],[3,1],[4,1],
                [0,2],[1,2],[2,2],[3,2],[4,2],
                [0,3],[1,3],[2,3],[3,3],[4,3]]


CHIP_LIST_4 = [[0,0],[1,0],[0,1],[1,1]]
spin3 = CHIP_LIST_4 #then, we can access like spin3[0], which corresponds to chip<0,0>, etc.

#spin5 = CHIP_LIST_20
spin5 = CHIP_LIST_48
#spin5 = CHIP_LIST_15    # Sepertinya ini dah cukup untuk small video -> butuh 9-ms aja!
#spin5 = CHIP_LIST_4
#SPINN3_HOST = '192.168.240.1'
SPINN3_HOST = '192.168.240.253'
SPINN5_HOST = '192.168.240.1'

# Which SpiNNaker board?
# spiNN = spin5
spiNN = spin3

DEF_SEND_PORT = 17893 #tidak bisa diganti dengan yang lain
# in the aplx, it is defined:
# define SDP_TAG_REPLY			1
# define SDP_TAG_RESULT			2
DEF_REPLY_PORT = 20000      # with tag 1
DEF_RESULT_PORT = 20001     # with tag 2
DEF_DEBUG_PORT = 20002      # with tag 3

chipX = 0
chipY = 0
sdpImgRedPort = 1       # based on the aplx code
sdpImgGreenPort = 2
sdpImgBluePort = 3
sdpImgConfigPort = 7
sdpAckPort = 6
SDP_CMD_CONFIG = 1
SDP_CMD_CONFIG_CHAIN = 11
SDP_CMD_PROCESS = 2
SDP_CMD_CLEAR = 3
SDP_CMD_ACK_RESULT = 4
IMG_OP_SOBEL_NO_FILT	= 1	# will be carried in arg2.low
IMG_OP_SOBEL_WITH_FILT	= 2	# will be carried in arg2.low
IMG_OP_LAP_NO_FILT		= 3	# will be carried in arg2.low
IMG_OP_LAP_WITH_FILT	= 4	# will be carried in arg2.low
IMG_NO_FILTERING		= 0
IMG_WITH_FILTERING		= 1
IMG_SOBEL				= 0
IMG_LAPLACE				= 1

# Ideally, SpiNNaker should inform host, who leads the app (leadAp)
# now, we assume that leadAp is in core 1
sdpCore = 1
pad = struct.pack('2B',0,0)

SEND_METHOD = 1 # broadcast to all chips
# SEND_METHOD = 0 # chip per chip

ASK_BLOCK_REPORT = 0
# ASK_BLOCK_REPORT = 1  # ask cores to report their working block region

class edgeGUI(QtGui.QWidget, mainGUI.Ui_pySpiNNEdge):
    # The following signals MUST defined here, NOT in the init()
    sdpUpdate = QtCore.pyqtSignal('QByteArray')  # for streaming data
    okToClose = True
    def __init__(self, def_image_dir = "./", parent=None):
        self.img_dir = def_image_dir
        self.img = None
        self.res = None
        self.continyu = True
        super(edgeGUI, self).__init__(parent)
        self.setupUi(self)

        self.orgViewPort = imgWidget("Original Image")
        #self.orgViewPort.show()
        self.resViewPort = imgWidget("Result SpiNNaker")
        #self.resViewPort.show()
        self.hostViewPort = imgWidget("Result PC-version")
        #self.hostViewPort.show()

        self.trx = trxIndicator()
        #self.trx.show()

        self.connect(self.pbLoad, QtCore.SIGNAL("clicked()"), QtCore.SLOT("pbLoadClicked()"))
        self.connect(self.pbSend, QtCore.SIGNAL("clicked()"), QtCore.SLOT("pbSendClicked()"))
        self.connect(self.pbProcess, QtCore.SIGNAL("clicked()"), QtCore.SLOT("pbProcessClicked()"))
        self.connect(self.pbSave, QtCore.SIGNAL("clicked()"), QtCore.SLOT("pbSaveClicked()"))
        self.connect(self.pbClear, QtCore.SIGNAL("clicked()"), QtCore.SLOT("pbClearClicked()"))
        #self.connect(self.cbSpiNN, QtCore.SIGNAL("currentIndexChanged(int)")), QtCore.SLOT("cbSpiNNChanged(int)")
        self.cbSpiNN.currentIndexChanged.connect(self.cbSpiNNChanged)

        if spiNN==spin3:
            self.cbSpiNN.setCurrentIndex(0)
        else:
            self.cbSpiNN.setCurrentIndex(1)
        self.cbSpiNNChanged(self.cbSpiNN.currentIndex())
        # self.sdpUpdate can be connected to other slot
        # self.sdpUpdate.connect(SOME_OTHER_SLOT)

        self.RptSock = QtNetwork.QUdpSocket(self)
        self.sdpSender = QtNetwork.QUdpSocket(self)
        self.initRptSock(DEF_RESULT_PORT)
        # self.initRptSock(DEF_REPLY_PORT)
        self.DbgSock = QtNetwork.QUdpSocket(self)
        self.initDbgSock(DEF_DEBUG_PORT)

        self.tResult = [0,0,0]

        self.debugVal = 0
        self.ResultTriggered = False

    @QtCore.pyqtSlot(int)
    def cbSpiNNChanged(self, idx):
        if idx==0:
            self.DEF_HOST = SPINN3_HOST
            print "Will use Spin3 with {}-nodes".format(len(spin3))
        else:
            self.DEF_HOST = SPINN5_HOST
            print "Will use Spin5 with {}-nodes".format(len(spin5))

    def closeEvent(self, event):
        if self.okToClose:
            self.orgViewPort.close()
            self.resViewPort.close()
            self.hostViewPort.close()
            self.trx.close()
            event.accept()
        else:
            event.ignore()


    def computeWLoad(self, nBlk, nCore):
        """
        Let's put in this table:
        | Blk | wID | sLine | eLine | addrIn | addrOut |
        """
        n = nBlk * nCore
        # idx = nCore + nCore*Blk
        self.wl = [[0 for _ in range(6)] for _ in range(n)]

    def initRptSock(self, port):
        print "Try opening port-{} for receiving result...".format(port),
        #result = self.sock.bind(QtNetwork.QHostAddress.LocalHost, DEF.RECV_PORT) --> problematik dengan penggunaan LocalHost
        result = self.RptSock.bind(port)
        if result is False:
            print 'failed! Cannot open UDP port-{}'.format(port)
        else:
            print "done!"
            self.RptSock.readyRead.connect(self.readRptSDP)

        # test kirim sendAck
        # self.sendAck(0)       # OK!

    @QtCore.pyqtSlot()
    def readRptSDP(self):
        # print "Got something..."
        if self.ResultTriggered is False:
            self.ResultTriggered = True
            #self.pktCntr = 1
            #print "Getting result..."
            #self.trx.setName("Receiving image...")
            #self.trx.setVal(0)
            #self.trx.show()

        szData = self.RptSock.pendingDatagramSize()
        datagram, host, port = self.RptSock.readDatagram(szData)
        self.sdpUpdate.emit(datagram)   # for additional processing
        # then call result retrieval processing
        if szData > 10:
            self.getImage(datagram)
            #print "{}:{}".format(self.pktCntr, szData)    # lihat, berapa byte yang dikirim
            #self.pktCntr += 1
        else:   # if aplx only send header through this UDP port, then it's finish sign
            if self.ProcessInProgress is False:
                self.ProcessInProgress = True
                print "SpiNNaker is starting the process...",
                self.tic = time.time()

            else:
                self.toc = time.time()
                #elapse = (mn - self.mn) * 60 + sc - self.sc
                elapse = int((self.toc - self.tic)*1000)

                sa = datagram[8:10]
                el = struct.unpack('H', sa)
                #print "done in {} millisecs!".format(elapse)
                print "done in {} millisecs!".format(el[0])
                self.ResultTriggered = False
                self.ProcessInProgress = False
                #self.trx.setVal(0)
                #self.trx.hide()
                self.processResult()

    def initDbgSock(self, port):
        print "Try opening port-{} for receiving debug info...".format(port),
        #result = self.sock.bind(QtNetwork.QHostAddress.LocalHost, DEF.RECV_PORT) --> problematik dengan penggunaan LocalHost
        result = self.DbgSock.bind(port)
        if result is False:
            print 'failed! Cannot open UDP port-{}'.format(port)
        else:
            print "done!"
            self.DbgSock.readyRead.connect(self.readDbgSDP)

    @QtCore.pyqtSlot()
    def readDbgSDP(self):
        # print "Got something..."
        while self.DbgSock.hasPendingDatagrams():
            szData = self.DbgSock.pendingDatagramSize()
            datagram, host, port = self.DbgSock.readDatagram(szData)
            self.processDebug(datagram, 4, 17)

    def processDebug(self, dgram, nBlk, nCore):
        """
        cmd_rc = Blk
        seq = wID
        arg1 = sLine
        arg2 = eLine
        arg3 = addr

        pad = dgram[0:2]      -> 2B
        flags = dgram[2:3]    -> B
        tag = dgram[3:4]      -> B
        dp = dgram[4:5]       -> B
        sp = dgram[5:6]       -> B
        da = dgram[6:8]       -> 2B
        sa = dgram[8:10]      -> 2B
        cmd = dgram[10:12]
        seq = dgram[12:14]
        arg1 = dgram[14:18]
        arg2 = dgram[18:22]
        arg3 = dgram[22:26]
        """

        # get srce:
        cmd = dgram[10:12]
        seq = dgram[12:14]
        arg1 = dgram[14:18]
        arg2 = dgram[18:22]
        arg3 = dgram[22:26]

        Blk = struct.unpack('<H', cmd)
        wID = struct.unpack('<H', seq)
        Line = struct.unpack('<I', arg1)
        addrIn = struct.unpack('<I', arg2)
        addrOut = struct.unpack('I', arg3)

        sLine = Line[0] >> 16
        eLine = Line[0] & 0xFFFF

        idx = wID[0] + nCore*Blk[0]

        self.wl[idx][0] = Blk[0]
        self.wl[idx][1] = wID[0]
        self.wl[idx][2] = sLine
        self.wl[idx][3] = eLine
        self.wl[idx][4] = addrIn[0]
        self.wl[idx][5] = addrOut[0]

        self.debugVal += 1

        # if all cores have been reporting, print the table
        misFound = False
        if self.debugVal == len(self.wl):
            """
            """
            inA = 0x61000000
            outA = 0x64000000
            print "Blk\twID\tsLine\teLine\taddrIn\taddrOut\tExpAddIn\tExpAddOut"
            print "-------------------------------------------------"
            for l in self.wl:
                print "%d\t%d\t%d\t%d\t0x%x\t0x%x\t0x%x\t0x%x" % (l[0], l[1], l[2], l[3], l[4], l[5], inA, outA)
                if inA != l[4] or outA != l[5]:
                    misFound = True
                inA += (l[3]-l[2]+1)*self.w
                outA += (l[3]-l[2]+1)*self.w
            if misFound == True:
                print "Found mismatch in address!!!"
            else:
                print "Region splitting seems to be correct!"

    def saveResult(self):
        """
        dump the content of self.res into files
        """
        fileR = open(self.fName + ".resR", 'wb')
        fileG = open(self.fName + ".resG", 'wb')
        fileB = open(self.fName + ".resB", 'wb')

        for y in range(self.h):
            r = self.res[0][y]
            g = self.res[1][y]
            b = self.res[2][y]
            fileR.write(r)
            fileG.write(g)
            fileB.write(b)
        fileR.close()
        fileG.close()
        fileB.close()

    def processResult(self):
        # print "Result is received completely!"
        self.saveResult()
        # print "Total pixel = ", self.tResult
        # return
        # How to render?
        imgResult = QtGui.QImage(self.w, self.h, QtGui.QImage.Format_RGB32)
        for y in range(self.h):
            for x in range(self.w):
                r = self.res[0][y][x]
                g = self.res[1][y][x]
                b = self.res[2][y][x]

                clr = QtGui.qRgb(r, g, b)
                imgResult.setPixel(x,y,clr)

        # Then display it on the second graphicsView
        resultPixmap = QtGui.QPixmap()
        #self.pixmap.fromImage(self.img)    #something not right with this fromImage() function
        resultPixmap.convertFromImage(imgResult)
        self.resViewPort.setPixmap(resultPixmap)
        self.resViewPort.show()


    def getImage(self, sdpData):
        """
        NOTE: Data format used by aplx is:
        msg->srce_addr = image line + rgb
        msg->srce_port = sequence number (starting from 1)
        msg->cmd_rc + msg->data = pixel data

        fmt = "<H4BH2B2H3I20B"
        pad, flags, tag, dp, sp, da, say, sax, cmd, seq, arg1, arg2, arg3, \
                cpu0, cpu1, cpu2, cpu3, cpu4, cpu5, cpu6, cpu7, cpu8, cpu9, cpu10, cpu11, \
                cpu12, cpu13, cpu14, cpu15, cpu16, cpu17, cpu18, cpu19 = struct.unpack(fmt, datagram)

        pad = sdpData[0:2]      -> 2B
        flags = sdpData[2:3]    -> B
        tag = sdpData[3:4]      -> B
        dp = sdpData[4:5]       -> B
        sp = sdpData[5:6]       -> B
        da = sdpData[6:8]       -> 2B
        sa = sdpData[8:10]      -> 2B
        cmd = sdpData[10:12]
        seq = sdpData[12:14]
        dst...
        """

        # get srce:
        srce_port = sdpData[5:6]
        srce_addr = sdpData[8:10]
        lData = len(sdpData)
        pixel = sdpData[10:lData]
        seq =  srce_port    # not used, because we use append
        blkID = struct.unpack('B', seq)
        srceAddr = struct.unpack('<H', srce_addr)   # will result in a tupple
        lines = srceAddr[0] >> 2
        rgb = srceAddr[0] & 3
        self.tResult[rgb] += len(pixel)
        # NOTE: in bytearray, we cannot simply use append, because it might produce ValueError: string must be of size 1
        self.res[rgb][lines]  = self.res[rgb][lines] + pixel
        # print "Got chunk from node-{}".format(blkID[0]-1)
        # then send acknowledge
        #self.trx.appendVal(len(pixel))
        # send Acknowledge:
        self.sendAck(0)
        # print "ack sent!"

    def sendAck(self, blkID):
        # Try with broadcasting method only
        # sendSDP(self,flags, tag, dp, dc, dax, day, cmd, seq, arg1, arg2, arg3, bArray):
        # dp = sdpImgConfigPort
        dpc = (sdpAckPort << 5) + sdpCore
        hdr = struct.pack('4B2H',0x07,0,dpc,255,0,0)
        reply = QtNetwork.QUdpSocket()
        self.sendSDP(0x07,0,sdpAckPort,sdpCore,0,0,0,0,0,0,0,None)
        #reply.writeDatagram(hdr, QtNetwork.QHostAddress(SPINN3_HOST), DEF_SEND_PORT)
        #self.RptSock.writeDatagram(hdr, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)

    @QtCore.pyqtSlot()
    def pbLoadClicked(self):
        self.fName = QtGui.QFileDialog.getOpenFileName(parent=self, caption="Load an image from file", \
                                                       directory = self.img_dir, filter = "*.*")
        #print self.fName    #this will contains the full path as well!
        if self.fName == "" or self.fName is None:
            return
        self.img = QtGui.QImage()
        self.img.load(self.fName)
        self.w = self.img.width()
        self.h = self.img.height()
        self.d = self.img.depth()
        self.c = self.img.colorCount()
        self.isGray = self.img.isGrayscale()
        print "Image width = {}, height = {}, depth = {}, clrCntr = {}".format(self.w,self.h,self.d,self.c)
        # For 16bpp or 32bpp (which is non-indexed), clrCntr is always 0

        # Convert to 32-bit format
        self.img32 = self.img.convertToFormat(QtGui.QImage.Format_RGB32)

        # Idea: use QImage for direct pixel manipulation and use QPixmap for rendering
        pixmap = QtGui.QPixmap()
        #self.pixmap.fromImage(self.img)    #something not right with this fromImage() function
        pixmap.convertFromImage(self.img32)
        #self.pixmap.load(self.fName) #pixmap.load() works just fine
        self.orgViewPort.setPixmap(pixmap)
        self.orgViewPort.show()

        # Then create 3maps for each color. The sobel/laplace operators will operate on these maps
        self.rmap0 = [[0 for _ in range(self.w)] for _ in range(self.h)]    # the original
        self.rmap1 = [[0 for _ in range(self.w)] for _ in range(self.h)]    # the result
        self.gmap0 = [[0 for _ in range(self.w)] for _ in range(self.h)]
        self.gmap1 = [[0 for _ in range(self.w)] for _ in range(self.h)]
        self.bmap0 = [[0 for _ in range(self.w)] for _ in range(self.h)]
        self.bmap1 = [[0 for _ in range(self.w)] for _ in range(self.h)]

        for x in range(self.w):
            for y in range(self.h):
                # jika pakai r = Qt.QColor.red(va) akan muncul error:
                # TypeError: QColor.red(): first argument of unbound method must have type 'QColor'
                # jadi memang harus di-instantiate!
                rgb = Qt.QColor(self.img32.pixel(x,y))
                self.rmap0[y][x] = rgb.red()    # remember, NOT rmap[x][y] -> because...
                self.gmap0[y][x] = rgb.green()
                self.bmap0[y][x] = rgb.blue()

        self.computeWLoad(4, 17)
        print "Image is loaded!"


    @QtCore.pyqtSlot()
    def pbSendClicked(self):
        #TODO:  1. save the rgbmap to a dummy binary file -> use pbSaveClicked()
        #       2. send to all chips using ybug->sload
        #       3. test by downloading using ybug->sdump
        if self.img is None:
            return

        self.pktCntr = 1
        if SEND_METHOD==0:
            # Experiment: use 4 chips:
            for chip in range(4):
                print "Sending img to chip <{},{}>...".format(spin3[chip][0], spin3[chip][1])
                self.sendImgPerChip(chip, 4)
        else:
            # TODO: how to broadcast data via scp? See TODO.FLOOD_FILL-howTo --> maybe NO, we use our own SDP
            exampleConf = dict()
            if self.cbSpiNN.currentIndex()==0:
                for i in range(len(spin3)):
                    exampleConf[i] = spin3[i]
                # exampleConf for spin3:
                #exampleConf[0]=[0,0]
                #exampleConf[1]=[1,0]
                #exampleConf[2]=[0,1]
                #exampleConf[3]=[1,1]
            else:
                for i in range(len(spin5)):
                    exampleConf[i] = spin5[i]
            self.sendBcastImg(exampleConf)

        # prepare the result
        self.res = [[bytearray() for _ in range(self.h)] for _ in range(3)]
        self.ProcessInProgress = False

    def sendBcastImg(self, blkDict):
        """
        Let's try broadcasting image to all chips
        #first, prepare SDP container and SDP receiver (acknowledge)
        #send image information through SDP_PORT_CONFIG
        #the data segment contains list of block-id with format:
        #[x,y,blockID1],[x,y,blockID2],etc
        #in this case, arg2 contains nodeBlockID for chip<0,0> and the maxBlock
        #arg3 is used to indicate if workers need to send their working region
        """
        print "Sending image...",
        dpcc = (sdpImgConfigPort << 5) + sdpCore    # TODO: in future, let aplx report leadAp first!
        hdrc = struct.pack('4B2H',0x07,1,dpcc,255,0,0)
        cmd = SDP_CMD_CONFIG_CHAIN
        if self.isGray is True:
            isGray = 1
        else:
            isGray = 0
        if self.cbMode.currentIndex()==0 and self.cbFilter.isChecked() is False:
            opt = 1
        elif self.cbMode.currentIndex()==0 and self.cbFilter.isChecked() is True:
            opt = 2
        elif self.cbMode.currentIndex()==1 and self.cbFilter.isChecked() is False:
            opt = 3
        else:
            opt = 4
        seq = (isGray << 8) + opt
        arg1 = (self.w << 16) + self.h
        arg3 = ASK_BLOCK_REPORT
        #arg3 = 1    # Let's see block distribution

        # find, what is the id of chip<0,0>
        chip0ID = 0
        ba = bytearray()
        for id in blkDict:
            if (blkDict[id][0] == blkDict[id][1]) and (blkDict[id][0] == 0):
                chip0ID = id
            else:
                # fill in the data segment, but don't include root node
                blkID = struct.pack("3B",blkDict[id][0], blkDict[id][1],id)
                ba = ba + blkID
        arg2 = (chip0ID << 16) + len(blkDict)  # arg2.high = nodeBlockID, arg2.low = maxBlock
        scp = struct.pack('2H3I',cmd,seq,arg1,arg2,arg3)   # arg3 is not used

        sdp = pad + hdrc + scp + ba
        udpSock = QtNetwork.QUdpSocket()
        udpSock.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)

        #time.sleep(1)   # wait a second, beri kesempatan spinnaker report work load via debugMsg

        """
        # Test sendAck
        for i in range(4):
            self.sendAck(i)
        """

        # then start the broadcast by sending image to chip<0,0>
        self.sendImg(0, 0)
        print "done!\n"

    def sendImgPerChip(self, chipIdx, nChip):
        """
        Send image data to a specific chip, where chipIdx = 0,1,2,3 for spin3.
        This function assume rmap0, gmap0, and bmap0 are ready!
        """
        # first, send image configuration
        # for spin3:
        x = spin3[chipIdx][0]
        y = spin3[chipIdx][1]
        da = (x << 8) + y

        # send image information through SDP_PORT_CONFIG
        dpcc = (sdpImgConfigPort << 5) + sdpCore
        hdrc = struct.pack('4B2H', 0x07, 1, dpcc, 255, da, 0)
        cmd = SDP_CMD_CONFIG
        if self.isGray is True:
            isGray = 1
        else:
            isGray = 0
        if self.cbMode.currentIndex() == 0 and self.cbFilter.isChecked() is False:
            opt = 1
        elif self.cbMode.currentIndex() == 0 and self.cbFilter.isChecked() is True:
            opt = 2
        elif self.cbMode.currentIndex() == 1 and self.cbFilter.isChecked() is False:
            opt = 3
        else:
            opt = 4
        seq = (isGray << 8) + opt
        arg1 = (self.w << 16) + self.h
        arg2 = (chipIdx << 16) + nChip  # arg2.high = nodeBlockID, arg2.low = maxBlock
        arg3 = ASK_BLOCK_REPORT

        scp = struct.pack('2H3I',cmd,seq,arg1,arg2,arg3)   # arg3 is not used
        sdp = pad + hdrc + scp
        self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)
        # this time, no need for reply

        time.sleep(1)   # wait a second
        print "Sending image..."

        self.sendImg(x, y)

    def sendImg(self, chipX, chipY):
        self.trx.show()
        self.trx.setName("Sending image...")
        # Cannot use self.sdpSender as the socket, since this sendImg() is blocking!
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.bind( ('', DEF_REPLY_PORT) )
        except OSError as msg:
            print "%s" % msg
            sock.close()
            sock = None

        if sock is None:
            print "Fail to open socket...!"

        # try sending R-layer (in future, grey mechanism must be considered)
        da = (chipX << 8) + chipY
        dpcr = (sdpImgRedPort << 5) + sdpCore
        dpcg = (sdpImgGreenPort << 5) + sdpCore
        dpcb = (sdpImgBluePort << 5) + sdpCore
        hdrr = struct.pack('4B2H',0x07,1,dpcr,255,da,0)
        hdrg = struct.pack('4B2H',0x07,1,dpcg,255,da,0)
        hdrb = struct.pack('4B2H',0x07,1,dpcb,255,da,0)

        # second, make a sequence (transforming 2D-map0 into 1D array)
        rArray = list()
        for y in range(self.h):
            for x in range(self.w):
                rArray.append(self.rmap0[y][x])

        self.szImg = len(rArray)
        if self.isGray is True:
            self.szImg *= 3
        self.trx.setMaxVal(self.szImg)

        pixelCntr = 0

        # third, iterate until all elements are sent
        # print "Sending R-channel",
        remaining = len(rArray)
        stepping = 16+256   # initially, it's size is SCP+data_part
        stopping = stepping
        starting = 0
        while remaining > 0:

            chunk = rArray[starting:stopping]
            ba = bytearray(chunk)
            """
            print "Sending {} pixels:".format(len(ba))
            for b in ba:
                print "%02x " % b,
            print
            """
            sdp = pad + hdrr + ba
            # self.continyu = False
            self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)

            # then waiting for a reply
            reply = sock.recv(UDP_BUFF_SIZE)   # cannot use self.continyu in readRptSDP()

            pixelCntr += len(ba)
            self.trx.setVal(pixelCntr)

            remaining -= stepping
            starting = stopping

            if remaining < stepping:
                stepping = remaining
            stopping += stepping

        # fourth, send an empty data to signify end of image transfer
        sdp = pad + hdrr
        self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)
        # print "done!"

        time.sleep(0.5)
        # check if the image is gray, if not, then send the green and blue layers
        # NOTE: kalau diselang-seling G dengan B, menyebabkan aplx RTE
        if self.isGray is False:
            gArray = list()
            bArray = list()
            for y in range(self.h):
                for x in range(self.w):
                    gArray.append(self.gmap0[y][x])
                    bArray.append(self.bmap0[y][x])

            # G-channel: iterate until all elements are sent
            # print "Sending G-channel...",
            remaining = len(gArray) #which is equal to len(bArray) as well
            stepping = 16+256   # initially, it's size is SCP+data_part
            stopping = stepping
            starting = 0
            while remaining > 0:

                chunk = gArray[starting:stopping]
                ba = bytearray(chunk)
                """
                print "Sending {} pixels:".format(len(ba))
                for b in ba:
                    print "%02x " % b,
                print
                """
                sdp = pad + hdrg + ba
                # self.continyu = False
                self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)

                reply = sock.recv(UDP_BUFF_SIZE)
                pixelCntr += len(ba)
                self.trx.setVal(pixelCntr)

                # repeat for the next chunk
                remaining -= stepping
                starting = stopping

                if remaining < stepping:
                    stepping = remaining
                stopping += stepping

            # finally, send an empty data to signify end of image transfer
            sdp = pad + hdrg
            self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)
            # print "done!\nSending B-channel...",

            time.sleep(0.5)
            # B-channel: iterate until all elements are sent
            remaining = len(bArray) #which is equal to len(bArray) as well
            stepping = 16+256   # initially, it's size is SCP+data_part
            stopping = stepping
            starting = 0
            while remaining > 0:

                chunk = bArray[starting:stopping]
                ba = bytearray(chunk)
                """
                print "Sending {} pixels:".format(len(ba))
                for b in ba:
                    print "%02x " % b,
                print
                """
                sdp = pad + hdrb + ba
                # self.continyu = False
                self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)

                reply = sock.recv(UDP_BUFF_SIZE)
                pixelCntr += len(ba)
                self.trx.setVal(pixelCntr)

                # repeat for the next chunk
                remaining -= stepping
                starting = stopping

                if remaining < stepping:
                    stepping = remaining
                stopping += stepping

            # finally, send an empty data to signify end of image transfer
            sdp = pad + hdrb
            self.sdpSender.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)
            # print "done!"

        sock.close()
        self.trx.hide()

    @QtCore.pyqtSlot()
    def pbProcessClicked(self):

        if self.img is None:
            return

        if self.cbMode.currentIndex() == 0:
            self.newImg = self.doSobel()
        elif self.cbMode.currentIndex()==1:
            self.newImg = self.doLaplace()

        #Then display it on the second graphicsView
        resultPixmap = QtGui.QPixmap()
        #self.pixmap.fromImage(self.img)    #something not right with this fromImage() function
        resultPixmap.convertFromImage(self.newImg)
        #self.pixmap.load(self.fName) #pixmap.load() works just fine
        self.hostViewPort.setPixmap(resultPixmap)
        self.hostViewPort.show()


    @QtCore.pyqtSlot()
    def pbSaveClicked(self):
        if self.img is None:
            return
        print "Saving layers as binary files...",
        rfName = self.fName + ".red"
        gfName = self.fName + ".green"
        bfName = self.fName + ".blue"
        rFile = open(rfName, 'wb')
        gFile = open(gfName, 'wb')
        bFile = open(bfName, 'wb')
        for y in range(self.h):
            r = bytearray()
            g = bytearray()
            b = bytearray()
            for x in range(self.w):
                r.append(self.rmap0[y][x])
                g.append(self.gmap0[y][x])
                b.append(self.bmap0[y][x])
            rFile.write(r)
            gFile.write(g)
            bFile.write(b)
        bFile.close()
        gFile.close()
        rFile.close()
        print "done!"

    @QtCore.pyqtSlot()
    def pbClearClicked(self):
        if self.img is None:
            return

        # clear
        self.img = None
        self.res = None
        self.ResultTriggered = False
        self.orgViewPort.hide()
        self.resViewPort.hide()
        self.hostViewPort.hide()


        #TODO: send notification to spinnaker that it is "clear"
        dc = sdpCore
        dp = sdpImgConfigPort
        cmd = SDP_CMD_CLEAR
        if SEND_METHOD==1:
            self.sendSDP(0x07, 0, dp, dc, chipX, chipY, cmd, 0, 0, 0, 0, None)
        else:
            for i in range(4):
                self.sendSDP(0x07, 0, dp, dc, spin3[i][0], spin3[i][1], cmd, 0, 0, 0, 0, None)

    def doFiltering(self):
        print "do filtering...",
        rgb = [0 for _ in range(3)]
        for y in range(self.h):
            for x in range(self.w):
                if y==0 or y==1 or y==(self.h-2) or y==(self.h-1):
                    #sumXY = [self.rmap0[y][x],self.gmap0[y][x],self.bmap0[y][x]]
                    sumXY = [0,0,0]
                elif x==0 or x==1 or x==(self.w-2) or x==(self.w-1):
                    #sumXY = [self.rmap0[y][x],self.gmap0[y][x],self.bmap0[y][x]]
                    sumXY = [0,0,0]
                else:
                    sumXY = [0,0,0]
                    for row in [-2,-1,0,1,2]:
                        for col in [-2,-1,0,1,2]:
                            sumXY[0] += self.rmap0[y+col][x+row]*FILT[col+2][row+2]    #red
                            sumXY[1] += self.gmap0[y+col][x+row]*FILT[col+2][row+2]    #green
                            sumXY[2] += self.bmap0[y+col][x+row]*FILT[col+2][row+2]    #blue

                for i in range(3):
                    rgb[i] = int(sumXY[i]/FILT_DENOM)
                    if rgb[i] > 255:
                        rgb[i] = 255
                    if rgb[i] < 0:
                        rgb[i] = 0
                self.rmap0[y][x] = rgb[0]
                self.gmap0[y][x] = rgb[1]
                self.bmap0[y][x] = rgb[2]
        print "done!"

    def doSobel(self):
        """
        Process using sobel operator. Takes rmap0,gmap0 and bmap0 as the operands.
        :return:
        """
        if self.cbFilter.isChecked() is True:
            self.doFiltering()

        print "Processing with Sobel filter...",
        imgSobel = QtGui.QImage(self.w, self.h, QtGui.QImage.Format_RGB32)
        rgb = [0 for _ in range(3)]
        for y in range(self.h):
            for x in range(self.w):
                sumX = [0,0,0]  # [r,g,b]map
                sumY = [0,0,0]
                sumXY = [0,0,0]
                if y==0 or y==(self.h-1):
                    sumXY = [0,0,0]
                elif x==0 or x==(self.w-1):
                    sumXY = [0,0,0]
                else:
                    for row in [-1,0,1]:
                        for col in [-1,0,1]:
                            sumX[0] += self.rmap0[y+col][x+row]*GX[col+1][row+1]    #red
                            sumX[1] += self.gmap0[y+col][x+row]*GX[col+1][row+1]    #green
                            sumX[2] += self.bmap0[y+col][x+row]*GX[col+1][row+1]    #blue
                            sumY[0] += self.rmap0[y+col][x+row]*GY[col+1][row+1]
                            sumY[1] += self.gmap0[y+col][x+row]*GY[col+1][row+1]
                            sumY[2] += self.bmap0[y+col][x+row]*GY[col+1][row+1]
                    sumXY[0] = math.sqrt(math.pow(sumX[0],2) + math.pow(sumY[0],2))
                    sumXY[1] = math.sqrt(math.pow(sumX[1],2) + math.pow(sumY[1],2))
                    sumXY[2] = math.sqrt(math.pow(sumX[2],2) + math.pow(sumY[2],2))

                for i in range(3):
                    rgb[i] = int(sumXY[i])
                    if rgb[i] > 255:
                        rgb[i] = 255
                    if rgb[i] < 0:
                        rgb[i] = 0
                self.rmap1[y][x] = 255-rgb[0]
                self.gmap1[y][x] = 255-rgb[1]
                self.bmap1[y][x] = 255-rgb[2]

                clr = QtGui.qRgb(self.rmap1[y][x],self.gmap1[y][x],self.bmap1[y][x])
                imgSobel.setPixel(x,y,clr)
        print "done!"
        return imgSobel

    def doLaplace(self):
        """
        Process using laplace operator
        :return:
        """
        if self.cbFilter.isChecked() is True:
            self.doFiltering()
        print "Processing with Laplace filter...",
        imgLaplace= QtGui.QImage(self.w, self.h, QtGui.QImage.Format_RGB32)

        rgb = [0 for _ in range(3)]
        for y in range(self.h):
            for x in range(self.w):
                sumXY = [0,0,0]
                if y==0 or y==1 or y==(self.h-2) or y==(self.h-1):
                    sumXY = [0,0,0]
                elif x==0 or x==1 or x==(self.w-2) or x==(self.w-1):
                    sumXY = [0,0,0]
                else:
                    for row in [-2,-1,0,1,2]:
                        for col in [-2,-1,0,1,2]:
                            sumXY[0] += self.rmap0[y+col][x+row]*LAP[col+2][row+2]    #red
                            sumXY[1] += self.gmap0[y+col][x+row]*LAP[col+2][row+2]    #green
                            sumXY[2] += self.bmap0[y+col][x+row]*LAP[col+2][row+2]    #blue

                for i in range(3):
                    rgb[i] = int(sumXY[i])
                    if rgb[i] > 255:
                        rgb[i] = 255
                    if rgb[i] < 0:
                        rgb[i] = 0
                self.rmap1[y][x] = 255-rgb[0]
                self.gmap1[y][x] = 255-rgb[1]
                self.bmap1[y][x] = 255-rgb[2]

                clr = QtGui.qRgb(self.rmap1[y][x],self.gmap1[y][x],self.bmap1[y][x])
                imgLaplace.setPixel(x,y,clr)
        print "done!"
        return imgLaplace

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
        CmdSock.writeDatagram(sdp, QtNetwork.QHostAddress(self.DEF_HOST), DEF_SEND_PORT)
        CmdSock.flush()
        return sdp


"""
To read the size of file:
coba = os.stat('nama_file')
print coba.st_size
"""
