cmake_minimum_required(VERSION 3.26.0)
project(proj01 VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)

# place for finding Findglfw3.cmake
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


# external opengl related libraries 
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)


# adding source files to our exectuable programs
# Note: it is not a good practice to put glad.c in the src folder
# Ideally, it should be put under external/ and used as an external library
# to avoid unnecessary compiling
add_executable(run01 src/main.cpp src/glad.c src/shader.cpp src/Mesh.cpp src/Spatial.cpp)

# specify include directories
target_include_directories(run01 PRIVATE 
	include
	)


# a hardcoded solution for assimp, only works on Windows
# Ideally, we should find one workable Findassimp.cmake, and use find_package(assimp)
FIND_LIBRARY(ASSIMP_LIBRARY NAMES assimp-vc143-mt
PATHS external/assimp/lib )

# for debugging
# message(ASSIMP_LIB="${ASSIMP_LIBRARY}")

# specify library directories
target_link_libraries(run01 ${GLFW3_LIBRARY} OpenGL::GL ${ASSIMP_LIBRARY})


# copy assimp dll, shaders and models
add_custom_command(
        TARGET run01 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
                "${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/lib/assimp-vc143-mt.dll"
                $<TARGET_FILE_DIR:run01>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                $<TARGET_FILE_DIR:run01>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/models"
                $<TARGET_FILE_DIR:run01>/models         
        )
