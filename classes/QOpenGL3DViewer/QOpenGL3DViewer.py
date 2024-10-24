# This Python file uses the following encoding: utf-8

"""
Author: Alejandro Torrejón Harto
Date: 24/10/2024
Description: This module implements a 3D viewer using OpenGL within a Qt widget. 
             It allows rendering cubes and points in a 3D space with camera controls 
             via mouse interaction (rotation, translation, and zoom).
             
####### USAGE ######
- In your QWidget, add this line to initialize the viewer:
    self.opengl_widget = QOpenGL3DViewer(FPS=60, parent=self)
    self.ui.viewer.addWidget(self.opengl_widget)

- You can use the following functions:
    set_cube(self, cube: [[float, float, float]]): 
        Sets the positions of cubes to be rendered.
    
    set_points(self, points: [[float, float, float]]):
        Sets the positions of points to be rendered.
    
    change_FPS(self, FPS: float):
        Changes the rendering frame rate.

Dependencies:
    - PySide6 for Qt Widgets and OpenGL support
    - OpenGL.GL and OpenGL.GLU for rendering
"""

from PySide6.QtCore import QTimer, Qt
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtGui import QVector2D
from OpenGL.GL import (glClearColor, glShadeModel, glMatrixMode,
                       glLoadIdentity, glPushMatrix, glPopMatrix,
                       glEnable, glViewport, glTranslatef, glRotatef,
                       glClear, glBegin, glEnd, glColor3f, glVertexPointer,
                       glVertex3f, glPointSize, glDrawArrays,glEnableClientState, 
                       glDisableClientState, )
from OpenGL.GL import (GL_DEPTH_TEST, GL_SMOOTH, GL_PROJECTION, GL_MODELVIEW,
                       GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_LINES, GL_FLOAT,
                       GL_POINTS, GL_VERTEX_ARRAY)
from OpenGL.GLU import gluPerspective

from time import time
import numpy as np

class QOpenGL3DViewer(QOpenGLWidget):
    """
    A Qt widget for 3D rendering using OpenGL. This viewer supports rendering 
    simple 3D objects such as cubes and points. The camera can be controlled 
    interactively using the mouse (rotation, translation, zoom).
    """

    def __init__(self, FPS=60, parent=None):
        """
        Initializes the QOpenGL3DViewer widget.

        Args:
            FPS (int): The desired frames per second for rendering. Defaults to 60.
            parent (QWidget, optional): The parent widget. Defaults to None.
        """
        super(QOpenGL3DViewer, self).__init__(parent)

        # Camera and interaction parameters
        self.angle_x = 0  # Rotation angle along X axis
        self.angle_y = 0  # Rotation angle along Y axis
        self.translate_x = 0  # Translation along X axis
        self.translate_y = 0  # Translation along Y axis
        self.zoom = -8.0  # Zoom level (along Z axis)
        self.last_mouse_position = QVector2D()  # Tracks last mouse position
        self.mouse_pressed = False  # Left mouse button state
        self.right_mouse_pressed = False  # Right mouse button state

        # Object positions
        self.cube_positions = np.zeros((0, 3), dtype=np.float32)  # List of cube positions
        self.cube_size = np.zeros((0, 2), dtype=np.float32)
        self.cube_rotations = np.zeros((0, 3), dtype=np.float32)
        self.cube_colors = np.zeros((0, 3), dtype=np.float32)
        self.points_positions = np.zeros((0, 3), dtype=np.float32)  # List of points positions

        # Render timer for continuous updating
        self.render_timer = QTimer(self)
        self.render_timer.timeout.connect(self.update)
        self.change_FPS(FPS)

    def initializeGL(self):
        """Initializes the OpenGL context and sets background color and depth test."""
        glClearColor(0.1, 0.1, 0.1, 1.0)  # Background color
        glEnable(GL_DEPTH_TEST)  # Enable depth testing
        glShadeModel(GL_SMOOTH)  # Smooth shading model

    def resizeGL(self, w, h):
        """
        Handles resizing of the OpenGL viewport.

        Args:
            w (int): Width of the viewport.
            h (int): Height of the viewport.
        """
        if h == 0:
            h = 1
        glViewport(0, 0, w, h)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(45, w / h, 0.1, 100.0)
        glMatrixMode(GL_MODELVIEW)

    def paintGL(self):
        """Renders the scene in the OpenGL context."""
        t = time()
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()

        # Apply camera transformations
        glTranslatef(self.translate_x, self.translate_y, self.zoom)
        glRotatef(self.angle_x, 1.0, 0.0, 0.0)  # Rotate around X axis
        glRotatef(self.angle_y, 0.0, 1.0, 0.0)  # Rotate around Y axis

        # Draw the coordinate axes
        self.draw_axes()

        # Render cubes at specified positions
        for pos, rot, size, color in zip(self.cube_positions, self.cube_rotations, self.cube_size, self.cube_colors):
            glPushMatrix()
            glTranslatef(*pos)
            glColor3f(*color)
            glRotatef(rot[0], 1, 0, 0)  # Rotación alrededor del eje X
            glRotatef(rot[1], 0, 1, 0)  # Rotación alrededor del eje Y
            glRotatef(rot[2], 0, 0, 1)  # Rotación alrededor del eje Z
            self.draw_cube(size)  # Llama a la función draw_cube con el tamaño actual
            glPopMatrix()

        # Render points at specified positions
        glColor3f(1.0, 1.0, 0.0)
        self.draw_points()

        print(f"\rDraw time: {1/(time()-t):.4f} FPS", end="")

    ################### DRAWERS ####################

    def draw_axes(self):
        """Draws the coordinate axes (X, Y, Z) in different colors."""
        glBegin(GL_LINES)

        # X axis (red)
        glColor3f(1.0, 0.0, 0.0)
        glVertex3f(0, 0, 0)
        glVertex3f(10, 0, 0)

        # Y axis (green)
        glColor3f(0.0, 1.0, 0.0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 10, 0)

        # Z axis (blue)
        glColor3f(0.0, 0.0, 1.0)
        glVertex3f(0, 0, 0)
        glVertex3f(0, 0, 10)

        glEnd()

    def draw_cube(self, size=np.array((1,1))):
        """Dibuja un cubo con bordes coloreados."""
        # Crea los vértices del cubo en función del tamaño
        half_size = size / 2.0
        vertices = np.array([
            [-half_size[0], -half_size[1], -half_size[2]], [half_size[0], -half_size[1], -half_size[2]],
            [half_size[0],  half_size[1], -half_size[2]], [-half_size[0],  half_size[1], -half_size[2]],
            [-half_size[0], -half_size[1],  half_size[2]], [half_size[0], -half_size[1],  half_size[2]],
            [half_size[0],  half_size[1],  half_size[2]], [-half_size[0],  half_size[1],  half_size[2]]
        ], dtype=np.float32)

        edges = [
            [0, 1], [1, 2], [2, 3], [3, 0],
            [4, 5], [5, 6], [6, 7], [7, 4],
            [0, 4], [1, 5], [2, 6], [3, 7]
        ]

        # Enable vertex array mode
        glEnableClientState(GL_VERTEX_ARRAY)

        # Define vertices for lines
        line_vertices = vertices[edges].reshape(-1, 3)  # Reshape para que se ajuste a glVertexPointer

        # Set vertex pointer to the line vertices
        glVertexPointer(3, GL_FLOAT, 0, line_vertices)

        # Draw the edges in one call
        glDrawArrays(GL_LINES, 0, line_vertices.shape[0])

        # Disable vertex array mode
        glDisableClientState(GL_VERTEX_ARRAY)
    
    def draw_points(self, size=2.0):
        """
        Función optimizada para dibujar puntos usando glDrawArrays para un mejor rendimiento.
        
        Args:
            size (float): El tamaño de los puntos a renderizar. Por defecto es 2.0.
        """
        if self.points_positions.size == 0:  # Verifica si hay puntos
            return

        # Establecer el tamaño de los puntos
        glPointSize(size)

        # Tiempo de ejecución de la conversión
        t = time()

        # Habilitar el modo de array de vértices
        glEnableClientState(GL_VERTEX_ARRAY)

        # Definir el array de vértices (3 coordenadas por punto)
        glVertexPointer(3, GL_FLOAT, 0, self.points_positions)

        # Dibujar todos los puntos en una sola llamada
        glDrawArrays(GL_POINTS, 0, len(self.points_positions))

        # Deshabilitar el modo de array de vértices
        glDisableClientState(GL_VERTEX_ARRAY)

        # print(f"\nTime taken for drawing: {time() - t}")

    #################### SETTERS ####################

    def set_cube(self, positions, sizes, rotations, colors):
        """Actualizar las posiciones, tamaños y rotaciones de los cubos."""
        self.cube_positions = np.array(positions, dtype=np.float32)  # Convertir a array de NumPy
        self.cube_size = np.array(sizes, dtype=np.float32)  # Asignar el tamaño de los cubos
        self.cube_rotations = np.array(rotations, dtype=np.float32)  # Convertir rotaciones a array de NumPy
        self.cube_colors = np.array(colors, dtype=np.float32)  # Convertir rotaciones a array de NumPy


    # def set_cube(self, cube):
    #     """
    #     Sets the positions for rendering cubes.

    #     Args:
    #         cube (list of [float, float, float]): A list of 3D positions where each cube will be placed.
    #     """
    #     self.cube_positions = cube

    def set_points(self, points):
        """
        Sets the positions for rendering points.

        Args:
            points (list of [float, float, float]): A list of 3D points to be rendered.
        """
        self.points_positions = points

    def change_FPS(self, FPS: float):
        """
        Adjusts the rendering frame rate.

        Args:
            FPS (float): The desired frame rate in frames per second.
        """
        if self.render_timer.isActive():
            self.render_timer.stop()
        self.render_timer.start(1000 / FPS)

    ################### MOUSE EVENTS ###################

    def mousePressEvent(self, event):
        """Handles mouse press events for rotation and translation."""
        if event.button() == Qt.MouseButton.LeftButton:
            self.mouse_pressed = True
            self.last_mouse_position = QVector2D(event.position())
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_pressed = True
            self.last_mouse_position = QVector2D(event.position())
        elif event.button() == Qt.MouseButton.MiddleButton:
            self.add_cube_at_center()

    def mouseMoveEvent(self, event):
        """Handles mouse movement for rotating or translating the scene."""
        current_position = QVector2D(event.position())
        delta = current_position - self.last_mouse_position

        if self.mouse_pressed:  # Left button for rotation
            self.angle_x += delta.y() * 0.5
            self.angle_y += delta.x() * 0.5
        elif self.right_mouse_pressed:  # Right button for translation
            self.translate_x += delta.x() * 0.01
            self.translate_y -= delta.y() * 0.01

        self.last_mouse_position = current_position

    def mouseReleaseEvent(self, event):
        """Handles mouse button release."""
        if event.button() == Qt.MouseButton.LeftButton:
            self.mouse_pressed = False
        elif event.button() == Qt.MouseButton.RightButton:
            self.right_mouse_pressed = False

    def wheelEvent(self, event):
        """Handles the mouse wheel for zooming in and out."""
        delta = event.angleDelta().y() / 120  # Wheel delta step
        self.zoom += delta * 0.5  # Adjust zoom level
