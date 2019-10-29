find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
	# The glesv2 package covers both OpenGL ES 2 and 3
	pkg_check_modules(OPENGLES glesv2)
endif()
