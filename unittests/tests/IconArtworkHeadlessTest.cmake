# Exercise the real wxGTK startup failure even on CI runners with a display.
# CTest controls the timeout, as it does for the native rendering tests.
foreach (require_gui IN ITEMS OFF ON)
	# Unsetting WAYLAND_DISPLAY still allows GTK to use wayland-0. Force
	# X11 so the missing DISPLAY guarantees startup fails on desktops too.
	set (environment GDK_BACKEND=x11 --unset=DISPLAY --unset=WAYLAND_DISPLAY
		--unset=AMULE_ICON_TEST_REQUIRE_GUI)
	set (expected_result 77)
	if (require_gui)
		list (APPEND environment AMULE_ICON_TEST_REQUIRE_GUI=1)
		set (expected_result 1)
	endif ()

	execute_process (
		COMMAND "${CMAKE_COMMAND}" -E env ${environment} "${ICON_ARTWORK_TEST}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE error
	)
	if (NOT "${result}" STREQUAL "${expected_result}")
		message (FATAL_ERROR
			"Headless icon test with require_gui=${require_gui}: expected exit "
			"${expected_result}, got ${result}\n${output}${error}")
	endif ()
endforeach ()
