PROJECT(KYLA_UI)

SET(SOURCES
	src/main.cpp

	src/SetupContext.cpp
	src/SplashDialog.cpp
	src/SetupDialog.cpp)

SET(HEADERS
	inc/SetupDialog.h
	inc/SplashDialog.h
	inc/SetupContext.h
	)

SET(MOC_SOURCES
	inc/SetupDialog.h
	inc/SplashDialog.h
	inc/SetupContext.h)

SET(UI_FILES
	ui/SetupDialog.ui
	ui/SplashDialog.ui)

FIND_PACKAGE(Qt5Core)
FIND_PACKAGE(Qt5Widgets)

# QT5_ADD_RESOURCES(OUT_RCC_FILES res/main.qrc)
QT5_WRAP_UI(OUT_UI_FILES ${UI_FILES})
QT5_WRAP_CPP(OUT_MOC_FILES ${MOC_SOURCES})

ADD_EXECUTABLE(kui WIN32
	${SOURCES}
	${HEADERS}
	${OUT_MOC_FILES}
	${OUT_UI_FILES})

TARGET_LINK_LIBRARIES(kui
	kyla Qt5::Core Qt5::Gui Qt5::Widgets)

TARGET_INCLUDE_DIRECTORIES(kui
	PRIVATE inc ${CMAKE_CURRENT_BINARY_DIR})
