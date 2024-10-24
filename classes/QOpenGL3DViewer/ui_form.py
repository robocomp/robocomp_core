# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'form.ui'
##
## Created by: Qt User Interface Compiler version 6.4.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QLabel, QSizePolicy, QSlider,
    QVBoxLayout, QWidget)

class Ui_Q3DViewer(object):
    def setupUi(self, Q3DViewer):
        if not Q3DViewer.objectName():
            Q3DViewer.setObjectName(u"Q3DViewer")
        Q3DViewer.resize(800, 600)
        self.verticalLayoutWidget = QWidget(Q3DViewer)
        self.verticalLayoutWidget.setObjectName(u"verticalLayoutWidget")
        self.verticalLayoutWidget.setGeometry(QRect(30, 100, 721, 341))
        self.viewer = QVBoxLayout(self.verticalLayoutWidget)
        self.viewer.setObjectName(u"viewer")
        self.viewer.setContentsMargins(0, 0, 0, 0)
        self.label = QLabel(Q3DViewer)
        self.label.setObjectName(u"label")
        self.label.setGeometry(QRect(330, 50, 67, 21))
        self.verticalLayoutWidget_2 = QWidget(Q3DViewer)
        self.verticalLayoutWidget_2.setObjectName(u"verticalLayoutWidget_2")
        self.verticalLayoutWidget_2.setGeometry(QRect(140, 480, 160, 80))
        self.verticalLayout = QVBoxLayout(self.verticalLayoutWidget_2)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.verticalLayout.setContentsMargins(0, 0, 0, 0)
        self.label_cube = QLabel(self.verticalLayoutWidget_2)
        self.label_cube.setObjectName(u"label_cube")

        self.verticalLayout.addWidget(self.label_cube)

        self.slider_cube = QSlider(self.verticalLayoutWidget_2)
        self.slider_cube.setObjectName(u"slider_cube")
        self.slider_cube.setOrientation(Qt.Horizontal)

        self.verticalLayout.addWidget(self.slider_cube)

        self.verticalLayoutWidget_3 = QWidget(Q3DViewer)
        self.verticalLayoutWidget_3.setObjectName(u"verticalLayoutWidget_3")
        self.verticalLayoutWidget_3.setGeometry(QRect(470, 490, 160, 80))
        self.verticalLayout_2 = QVBoxLayout(self.verticalLayoutWidget_3)
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.verticalLayout_2.setContentsMargins(0, 0, 0, 0)
        self.label_points = QLabel(self.verticalLayoutWidget_3)
        self.label_points.setObjectName(u"label_points")

        self.verticalLayout_2.addWidget(self.label_points)

        self.slider_points = QSlider(self.verticalLayoutWidget_3)
        self.slider_points.setObjectName(u"slider_points")
        self.slider_points.setMaximum(50000)
        self.slider_points.setOrientation(Qt.Horizontal)

        self.verticalLayout_2.addWidget(self.slider_points)


        self.retranslateUi(Q3DViewer)

        QMetaObject.connectSlotsByName(Q3DViewer)
    # setupUi

    def retranslateUi(self, Q3DViewer):
        Q3DViewer.setWindowTitle(QCoreApplication.translate("Q3DViewer", u"Q3DViewer", None))
        self.label.setText(QCoreApplication.translate("Q3DViewer", u"Q3DViewer", None))
        self.label_cube.setText(QCoreApplication.translate("Q3DViewer", u"Cubes", None))
        self.label_points.setText(QCoreApplication.translate("Q3DViewer", u"Points", None))
    # retranslateUi

