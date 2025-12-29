cmake_minimum_required(VERSION 3.7)

project(CreateMeshes)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CREATEMESHES_INSTALL_DIR ${CMAKE_BINARY_DIR})

# Add OpenGL, GLFW
find_package(OpenGL REQUIRED)
add_subdirectory(include/glfw)

# NVIDIA video codec libs
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        Set(CUVID_LIB ${LIB_DIR}/x64/nvcuvid.lib) 
        set(NVENCODEAPI_LIB ${LIB_DIR}/x64/nvencodeapi.lib)  
    else()
        Set(CUVID_LIB ${LIB_DIR}/Win32/nvcuvid.lib) 
        set(NVENCODEAPI_LIB ${LIB_DIR}/Win32/nvencodeapi.lib) 
    endif()
else ()
    find_library(CUVID_LIB nvcuvid)
    find_library(NVENCODEAPI_LIB nvidia-encode)
endif()

# FFMPEG LibAV libs
if(WIN32)
    set(NV_FFMPEG_HDRS ${INCLUDE_DIR}/FFmpeg/include)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        Set(AVCODEC_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/avcodec.lib)
        Set(AVFORMAT_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/avformat.lib)
        Set(AVUTIL_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/avutil.lib)
        Set(SWRESAMPLE_LIB ${INCLUDE_DIR}/FFmpeg/lib/x64/swresample.lib)
        Set(FFMPEG_DLL_DIR ${INCLUDE_DIR}/FFmpeg/lib/x64/)
    else()
        Set(AVCODEC_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/avcodec.lib)
        Set(AVFORMAT_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/avformat.lib)
        Set(AVUTIL_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/avutil.lib)
        Set(SWRESAMPLE_LIB ${INCLUDE_DIR}/FFmpeg/lib/Win32/swresample.lib)
        Set(FFMPEG_DLL_DIR ${INCLUDE_DIR}/FFmpeg/lib/Win32/)
    endif()
    #copy dll's
    install(DIRECTORY ${FFMPEG_DLL_DIR} DESTINATION ${CREATEMESHES_INSTALL_DIR} FILES_MATCHING PATTERN "*.dll")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PC_AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(PC_AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(PC_AVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(PC_SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

    set(NV_FFMPEG_HDRS ${PC_AVCODEC_INCLUDE_DIRS})
    find_library(AVCODEC_LIBRARY NAMES avcodec
            HINTS
            ${PC_AVCODEC_LIBDIR}
            ${PC_AVCODEC_LIBRARY_DIRS}
            )
    find_library(AVFORMAT_LIBRARY NAMES avformat
            HINTS
            ${PC_AVFORMAT_LIBDIR}
            ${PC_AVFORMAT_LIBRARY_DIRS}
            )
    find_library(AVUTIL_LIBRARY NAMES avutil
            HINTS
            ${PC_AVUTIL_LIBDIR}
            ${PC_AVUTIL_LIBRARY_DIRS}
            )
    find_library(SWRESAMPLE_LIBRARY NAMES swresample
            HINTS
            ${PC_SWRESAMPLE_LIBDIR}
            ${PC_SWRESAMPLE_LIBRARY_DIRS}
            )
    set(AVCODEC_LIB ${AVCODEC_LIBRARY})
    set(AVFORMAT_LIB ${AVFORMAT_LIBRARY})
    set(AVUTIL_LIB ${AVUTIL_LIBRARY})
    set(SWRESAMPLE_LIB ${SWRESAMPLE_LIBRARY})
endif()


# Source files
file(GLOB SRC_FILES src/*.cpp src/*.h include/glad/gl.c)
file(GLOB IMGUI_SOURCES src/imgui/*.cpp)
file(GLOB SHADER_FILES src/shaders/*.*)
source_group("imgui" FILES ${IMGUI_SOURCES} )
source_group("shaders" FILES ${SHADER_FILES})

# CUDA
find_package(CUDA)
set(CUDA_HOST_COMPILER ${CMAKE_CXX_COMPILER})
set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_50,code=\"sm_50,compute_50\")
if ( CMAKE_COMPILER_IS_GNUCC )
    if(NOT "${CUDA_NVCC_FLAGS}" MATCHES "-std=c\\+\\+11" )
        list(APPEND CUDA_NVCC_FLAGS -std=c++11)
    endif()
endif()
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()


cuda_add_executable(${PROJECT_NAME} ${SRC_FILES} ${IMGUI_SOURCES} ${SHADER_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

# Include headers
target_include_directories(${PROJECT_NAME} PUBLIC 
 ${CUDA_INCLUDE_DIRS}
 include/cxxopts 
 include/FastQuadricMeshSimplification
 ${NV_FFMPEG_HDRS}
 include/glad 
 include/glm 
 include/imgui 
 include/nlohmann 
 include/nvdec 
 include/stb_image 
)

target_link_libraries(${PROJECT_NAME} 
 OpenGL::GL glfw 
 ${CUDA_CUDA_LIBRARY} ${CMAKE_DL_LIBS}
 ${NVENCODEAPI_LIB} ${CUVID_LIB}
 ${AVCODEC_LIB} ${AVFORMAT_LIB} ${AVUTIL_LIB} ${SWRESAMPLE_LIB}
)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CREATEMESHES_INSTALL_DIR})

if (MSVC)
	# set startup project
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	# copy .libs and .dlls to the CONFIG directory after building
	set_target_properties( ${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CREATEMESHES_INSTALL_DIR}/$<CONFIG>/ )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD  COMMAND ${CMAKE_COMMAND} -E copy_directory ${FFMPEG_DLL_DIR} ${CREATEMESHES_INSTALL_DIR}/$<CONFIG>/)
endif (MSVC)

target_compile_definitions(${PROJECT_NAME} PUBLIC CMAKELISTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
