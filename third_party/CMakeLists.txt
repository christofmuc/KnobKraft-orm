


set(GIN_SOURCES
	${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin/gin.cpp
)

add_library(gin ${GIN_SOURCES})
target_link_libraries(gin PRIVATE juce-static)
target_include_directories(gin INTERFACE ${CMAKE_CURRENT_LIST_DIR}/Gin/modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Gin/modules/gin/utilities)
