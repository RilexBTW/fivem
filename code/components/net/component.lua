links { "ws2_32" }

return function()
	filter {}

	add_dependencies {
		'ros-patches',
		"vendor:citizen_enet",
		"vendor:citizen_util",
	}
end
