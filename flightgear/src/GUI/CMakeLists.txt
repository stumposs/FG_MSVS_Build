include(FlightGearComponent)

set(SOURCES
	AirportList.cxx
	CanvasWidget.cxx
	MapWidget.cxx
	WaypointList.cxx
	dialog.cxx
	FGPUIDialog.cxx
	fonts.cxx
	gui.cxx
	gui_funcs.cxx
	layout-props.cxx
	layout.cxx
	menubar.cxx
	FGPUIMenuBar.cxx
	new_gui.cxx
	property_list.cxx
	FGFontCache.cxx
	FGColor.cxx
    FileDialog.cxx
    PUIFileDialog.cxx
    MouseCursor.cxx
	)

set(HEADERS
	AirportList.hxx
	CanvasWidget.hxx
	MapWidget.hxx
	WaypointList.hxx
	dialog.hxx
	FGPUIDialog.hxx
	gui.h
	layout.hxx
	menubar.hxx
	FGPUIMenuBar.hxx
	new_gui.hxx
	property_list.hxx
	FGFontCache.hxx
	FGColor.hxx
    FileDialog.hxx
    PUIFileDialog.hxx
    MouseCursor.hxx
	)

if(WIN32)
	message(STATUS "on Windows")

	list(APPEND HEADERS WindowsMouseCursor.hxx)
	list(APPEND SOURCES WindowsMouseCursor.cxx)
endif()
		
if (APPLE)
    list(APPEND HEADERS FGCocoaMenuBar.hxx CocoaFileDialog.hxx CocoaMouseCursor.hxx)
    list(APPEND SOURCES FGCocoaMenuBar.mm CocoaFileDialog.mm CocoaMouseCursor.mm)
endif()
    		
flightgear_component(GUI "${SOURCES}" "${HEADERS}")
