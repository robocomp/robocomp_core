# This Python file uses the following encoding: utf-8


'''
#######USE######
- In your QWidget add this line
self.opengl_widget = OpenGL3DViewer(FPS=60, parent=self)
self.ui.viewer.addWidget(self.opengl_widget)

- You can use this functions
    set_cube(self, cube:[[float, float, float]])
    set_points(self, points:[[float, float, float]]):
    change_FPS(self, FPS:float):
'''
from PySide6.QtCore import QTimer, Qt
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtGui import QVector2D
from OpenGL.GL import (glClearColor, glShadeModel, glMatrixMode,
                        glLoadIdentity, glPushMatrix, glPopMatrix,
                        glEnable, glViewport, glTranslatef, glRotatef,
                        glClear, glBegin, glEnd, glVertex3fv, glColor3f,
                        glVertex3f, glPointSize)
from OpenGL.GL import (GL_DEPTH_TEST, GL_SMOOTH, GL_PROJECTION, GL_MODELVIEW,
                        GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_LINES,
                        GL_POINTS)
from OpenGL.GLU import gluPerspective


class QOpenGL3DViewer(QOpenGLWidget):
    def __init__(self, FPS=60, parent=None):
        super(QOpenGL3DViewer, self).__init__(parent)

        self.angle_x = 0  # Ángulo de rotación en el eje X
        self.angle_y = 0  # Ángulo de rotación en el eje Y
        self.translate_x = 0  # Traslación en el eje X
        self.translate_y = 0  # Traslación en el eje Y
        self.zoom = -8.0  # Nivel de zoom (desplazamiento en Z)
        self.last_mouse_position = QVector2D()  # Última posición del ratón
        self.mouse_pressed = False  # Estado del botón izquierdo del ratón
        self.right_mouse_pressed = False  # Estado del botón derecho del ratón

        # Lista de posiciones de cubos y esferas
        self.cube_positions = []  # 5 cubos alineados
        self.points_positions = []  # 5 esferas alineadas

        self.render_timer = QTimer(self)
        self.render_timer.timeout.connect(self.update)
        self.change_FPS(FPS)

    def initializeGL(self):
        glClearColor(0.1, 0.1, 0.1, 1.0)  # Color de fondo
        glEnable(GL_DEPTH_TEST)  # Habilitar test de profundidad
        glShadeModel(GL_SMOOTH)  # Sombreado suave

    def resizeGL(self, w, h):
        if h == 0:
            h = 1
        glViewport(0, 0, w, h)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45, w / h, 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)

    def paintGL(self):
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()

        # Configurar la cámara con zoom y traslación
        glTranslatef(self.translate_x, self.translate_y, self.zoom)

        # Rotar la escena según el movimiento del ratón
        glRotatef(self.angle_x, 1.0, 0.0, 0.0)  # Rotación en X
        glRotatef(self.angle_y, 0.0, 1.0, 0.0)  # Rotación en Y

        self.draw_axes()

        # Dibujar cubos en posiciones dinámicas
        for pos in self.cube_positions:
            glPushMatrix()
            glTranslatef(*pos)
            self.draw_cube()
            glPopMatrix()

        glPointSize(0.5)  # Tamaño de los puntos
        glBegin(GL_POINTS)
        for pos in self.points_positions:
            glColor3f(1.0, 1.0, 0.0)  # Color de los puntos
            glVertex3f(*pos)
        glEnd()


######################DRAW BODIES######################
    def draw_axes(self):
        """Función para dibujar los ejes X, Y, Z."""
        glBegin(GL_LINES)

        # Eje X en rojo
        glColor3f(1.0, 0.0, 0.0)  # Rojo
        glVertex3f(0, 0, 0)
        glVertex3f(10, 0, 0)

        # Eje Y en verde
        glColor3f(0.0, 1.0, 0.0)  # Verde
        glVertex3f(0, 0, 0)
        glVertex3f(0, 10, 0)

        # Eje Z en azul
        glColor3f(0.0, 0.0, 1.0)  # Azul
        glVertex3f(0, 0, 0)
        glVertex3f(0, 0, 10)

        glEnd()

    def draw_cube(self):
        """Función para dibujar un cubo."""
        vertices = [
            [-1.0, -1.0, -1.0], [1.0, -1.0, -1.0],
            [1.0,  1.0, -1.0], [-1.0,  1.0, -1.0],
            [-1.0, -1.0,  1.0], [1.0, -1.0,  1.0],
            [1.0,  1.0,  1.0], [-1.0,  1.0,  1.0]
        ]

        edges = [
            [0, 1], [1, 2], [2, 3], [3, 0],
            [4, 5], [5, 6], [6, 7], [7, 4],
            [0, 4], [1, 5], [2, 6], [3, 7]
        ]

        glBegin(GL_LINES)
        glColor3f(0.0, 1.0, 1.0)
        for edge in edges:
            for vertex in edge:
                glVertex3fv(vertices[vertex])
        glEnd()

    def draw_points(self, size=2.0):
        glPointSize(size) # Tamaño de los puntos
        glBegin(GL_POINTS)
        for pos in self.positions:
            glColor3f(1.0, 1.0, 0.0)  # Color de los puntos
            glVertex3f(*pos)
            glEnd()

#########################SET LIST###################################

    def set_cube(self, cube:[[float, float, float]]):
        self.cube_positions = cube

    def set_points(self, points:[[float, float, float]]):
        self.points_positions = points

    def change_FPS(self, FPS:float):
        if self.render_timer.isActive():
            self.render_timer.stop()
        self.render_timer.start(1/FPS)

    ###########################MOUSE EVENTS######################################
    def mousePressEvent(self, event):
        """Manejar cuando se presiona el botón del ratón."""
        if event.button() == Qt.MouseButton.LeftButton:
            self.mouse_pressed = True
            self.last_mouse_position = QVector2D(event.position())
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_pressed = True
            self.last_mouse_position = QVector2D(event.position())
        elif event.button() == Qt.MouseButton.MiddleButton:
            # Al pulsar la rueda del ratón, agregar un cubo nuevo en (0, 0, 0)
            self.add_cube_at_center()

    def add_cube_at_center(self):
        """Añadir un cubo nuevo en el centro (0, 0, 0) de la escena."""
        new_cube_position = (0, 0, 0)  # Centro de la escena
        self.cube_positions.append(new_cube_position)  # Añadir a la lista

    def mouseMoveEvent(self, event):
        """Manejar el movimiento del ratón."""
        current_position = QVector2D(event.position())
        delta = current_position - self.last_mouse_position

        if self.mouse_pressed:  # Rotación con botón izquierdo
            self.angle_x += delta.y() * 0.5  # Rotar en X
            self.angle_y += delta.x() * 0.5  # Rotar en Y

        elif self.right_mouse_pressed:  # Traslación con botón derecho
            self.translate_x += delta.x() * 0.01  # Trasladar en X
            self.translate_y -= delta.y() * 0.01  # Trasladar en Y (invertido)

        self.last_mouse_position = current_position

    def mouseReleaseEvent(self, event):
        """Manejar cuando se suelta el botón del ratón."""
        if event.button() == Qt.MouseButton.LeftButton:
            self.mouse_pressed = False
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_pressed = False

    def wheelEvent(self, event):
        """Manejar la rueda del ratón para hacer zoom."""
        delta = event.angleDelta().y() / 120  # Cada paso de la rueda es 120
        self.zoom += delta * 0.5  # Ajustar zoom (velocidad controlada)
