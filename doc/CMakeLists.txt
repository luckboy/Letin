find_package(PANDOC)

file(GLOB markdown_names RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.md")

if(PANDOC_FOUND)
	foreach(markdown_name IN LISTS markdown_names)
		string(REGEX REPLACE "^(.*).md$" "\\1.html" html_name "${markdown_name}")
		set(markdown_file "${CMAKE_CURRENT_SOURCE_DIR}/${markdown_name}")
		set(html_file "${CMAKE_CURRENT_BINARY_DIR}/${html_name}")
		add_custom_command(OUTPUT "${html_file}"
			COMMAND "${PANDOC_EXECUTABLE}" --template "${CMAKE_CURRENT_SOURCE_DIR}/template.html" -f markdown -t html -o "${html_file}" "${markdown_file}"
			DEPENDS "${markdown_file}" "${CMAKE_CURRENT_SOURCE_DIR}/template.html" )
		list(APPEND html_files "${html_file}")
	endforeach(markdown_name)

	add_custom_target(html ALL DEPENDS ${html_files})
endif(PANDOC_FOUND)

install(FILES ${markdown_names} DESTINATION share/doc)

if(PANDOC_FOUND)
	install(FILES ${html_files} DESTINATION share/doc)
endif(PANDOC_FOUND)
