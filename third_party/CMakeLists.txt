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

add_subdirectory(fmt)