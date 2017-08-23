#!/usr/bin/python
"""
This is the GUI for Edge Detector.
"""

from PyQt4 import Qt
import sys
from QtForms import edgeGUI

DEF_IMAGE_DIR = "../../../images"

if __name__=="__main__":
    if len(sys.argv) < 2:
        print "Usage: ./pySpiNNEdge <N_nodes>"
        sys.exit(-1)
    else:
        N = int(sys.argv[1])
        app = Qt.QApplication(sys.argv)
        gui = edgeGUI(def_image_dir=DEF_IMAGE_DIR, jmlNode=N)
        gui.show()
        sys.exit(app.exec_())

