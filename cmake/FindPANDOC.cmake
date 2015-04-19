set(PANDOC_FOUND OFF)

find_program(PANDOC_EXECUTABLE pandoc)

if(PANDOC_EXECUTABLE)
	set(PANDOC_FOUND ON)
else(PANDOC_EXECUTABLE)
	if(PANDOC_FIND_REQUIRED)
		message(FATAL_ERROR "Could not found pandoc")
	else(PANDOC_FIND_REQUIRED)
		message(STATUS "Could not found pandoc")
	endif(PANDOC_FIND_REQUIRED)
endif(PANDOC_EXECUTABLE)
