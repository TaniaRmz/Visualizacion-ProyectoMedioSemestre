cmake_minimum_required(VERSION 3.24)
project(visualizador)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(visualizador src/main.cpp src/modelo.cpp)
target_link_libraries(visualizador PRIVATE OpenGL::GL GLEW::GLEW glfw glm::glm)
target_compile_options(visualizador PRIVATE -Wall -Wextra)

# Así se encuentran los OBJ aunque ejecutemos desde la carpeta build.
# Al mover el proyecto a otra computadora hay que volver a compilarlo.
target_compile_definitions(visualizador PRIVATE
    CARPETA_MODELOS="${CMAKE_CURRENT_SOURCE_DIR}")
