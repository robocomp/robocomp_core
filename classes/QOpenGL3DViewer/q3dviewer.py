# This Python file uses the following encoding: utf-8
import sys

from PySide6.QtWidgets import QApplication, QWidget

# Important:
# You need to run the following command to generate the ui_form.py file
#     pyside6-uic form.ui -o ui_form.py, or
#     pyside2-uic form.ui -o ui_form.py
from ui_form import Ui_Q3DViewer

from PySide6.QtCore import QTimer

import random
from QOpenGL3DViewer import QOpenGL3DViewer


class Q3DViewer(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.ui = Ui_Q3DViewer()
        self.ui.setupUi(self)

        ###ADD In de componet
        self.opengl_widget = QOpenGL3DViewer(FPS=60, parent=self)
        self.ui.viewer.addWidget(self.opengl_widget)
        ###

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.generate_things)
        self.timer.start(16)


    def generate_things(self):
            """Generar 100 esferas alrededor de los cubos."""
            cube = []
            points = []

            distance = 20

            for _ in range(self.ui.slider_cube.value()):
                cube.append((random.uniform(-0, 10), random.uniform(-0, 10), random.uniform(-0, 10)))

            for _ in range(self.ui.slider_points.value()):
                x = random.uniform(-distance, distance)
                y = random.uniform(-distance, distance)
                z = random.uniform(-distance, distance)
                points.append([x, y, z])

            self.opengl_widget.set_cube(cube)
            self.opengl_widget.set_points(points)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    widget = Q3DViewer()
    widget.show()
    sys.exit(app.exec())
