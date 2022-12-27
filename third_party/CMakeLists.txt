if(APPLE)
set(GIN_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin/gin.mm
	${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin_gui/gin_gui.mm
)
else()
set(GIN_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin/gin.cpp
	${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin_gui/gin_gui.cpp
)
endif()

add_library(gin ${GIN_SOURCES})
target_link_libraries(gin PRIVATE juce-static)
target_include_directories(gin PUBLIC ${CMAKE_CURRENT_LIST_DIR}/Gin/modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin/utilities)

file(GLOB DOCKS_SOURCES "${CMAKE_CURRENT_LIST_DIR}/docks/docks/source/*.*")
add_library(docks STATIC "${DOCKS_SOURCES}")
#add_library(docks STATIC "${CMAKE_CURRENT_LIST_DIR}/docks/docks/docks.cpp" "${CMAKE_CURRENT_LIST_DIR}/docks/docks/docks.h")
file(GLOB DOCKS_RESOURCES "${CMAKE_CURRENT_LIST_DIR}/docks/docks/images/*")
juce_add_binary_data(docks_res SOURCES ${DOCKS_RESOURCES})
target_compile_definitions(docks PRIVATE "JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1")
target_include_directories(docks PUBLIC "${CMAKE_CURRENT_LIST_DIR}/docks" PRIVATE "${CMAKE_CURRENT_LIST_DIR}/docks/docks" "${CMAKE_CURRENT_LIST_DIR}/JUCE/modules" "${CMAKE_CURRENT_LIST_DIR}/JUCE/")
target_link_libraries(docks PUBLIC docks_res juce-utils)
