"""
This is the GUI Image Decoder.
"""

from PyQt4 import Qt
import sys
from QtForms import imgdecGUI

DEF_IMAGE_DIR = "../../../images"

if __name__=="__main__":
    app = Qt.QApplication(sys.argv)
    gui = imgdecGUI(def_image_dir=DEF_IMAGE_DIR)
    gui.show()
    sys.exit(app.exec_())

