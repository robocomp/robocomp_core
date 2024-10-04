cmake_minimum_required(VERSION 3.16)

project(GRAFCETExample)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Automatización de procesos de Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON) # Si tienes archivos .ui
set(CMAKE_AUTORCC ON) # Si tienes archivos .qrc

# Si necesitas añadir rutas adicionales donde CMake buscará
# list(APPEND CMAKE_PREFIX_PATH "tu/ruta_adicional")

# Encuentra el paquete de Qt6
find_package(Qt6 COMPONENTS Core Widgets StateMachine REQUIRED)

# Añade tu ejecutable
add_executable(${PROJECT_NAME} main.cpp GRAFCETExample.cpp GRAFCETStep.cpp)


# Enlaza las bibliotecas de Qt6 a tu ejecutable
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::StateMachine  # Asegurándonos de enlazar StateMachine
)