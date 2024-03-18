CMAKE_MINIMUM_REQUIRED( VERSION 3.20 )
project(carol-engine)

set(CMAKE_CXX_STANDARD 20)

foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin/lib)
endforeach()


file(GLOB_RECURSE carol-renderer-source ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/*.cpp)
file(GLOB win32-source 
    ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/win32/*.ico
    ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/win32/*.aps
    ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/win32/*.rc)
file(GLOB include-list LIST_DIRECTORIES true ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/include/*)
set(carol-renderer-include)
list(APPEND carol-renderer-include ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/include)

foreach(inc IN LISTS include-list)
    if (IS_DIRECTORY ${inc})
        list(APPEND carol-renderer-include ${inc})
    endif()
endforeach()


add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/assimp EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/DirectXTex EXCLUDE_FROM_ALL)

add_custom_target(copy-shader)
add_custom_command(
    TARGET copy-shader
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
    ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/shader
    ${CMAKE_BINARY_DIR}/bin/shader)

add_custom_target(copy-texture)
add_custom_command(
    TARGET copy-texture
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
    ${CMAKE_CURRENT_LIST_DIR}/carol_renderer/texture
    ${CMAKE_BINARY_DIR}/bin/texture)

add_executable(carol-engine WIN32 ${carol-renderer-source} ${win32-source})
target_include_directories(carol-engine PUBLIC ${carol-renderer-include})
target_link_libraries(carol-engine PUBLIC assimp DirectXTex)
target_link_libraries(carol-engine PUBLIC d3d12 dxgi dxguid)

add_dependencies(carol-engine copy-shader)
add_dependencies(carol-engine copy-texture)
