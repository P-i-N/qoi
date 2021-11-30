workspace "QOI"
	-- Premake output folder
	location(path.join(".build", _ACTION))

	-- Target architecture
	architecture "x86_64"

	-- Configuration settings
	configurations { "Debug", "Release" }

	-- Debug configuration
	filter { "configurations:Debug" }
		defines { "DEBUG" }
		symbols "On"
		optimize "Off"

	-- Release configuration
	filter { "configurations:Release" }
		defines { "NDEBUG" }
		optimize "Speed"
		inlining "Auto"

	filter { "language:not C#" }
		defines { "_CRT_SECURE_NO_WARNINGS" }
		characterset ("MBCS")
		buildoptions { "/std:c++latest" }

	filter { }
		targetdir ".bin/%{cfg.longname}/"
		defines { "WIN32", "_AMD64_" }
		vectorextensions "AVX2"

--debugdir "data"

-- Setup vcpkg dirs
includedirs { "$(VcpkgRoot)..\\x64-windows-static\\include" }
libdirs { "$(VcpkgRoot)..\\x64-windows-static\\lib" }

includedirs { "libs/stb" }

project "qoiconv"
	language "C++"
	kind "ConsoleApp"
	files { "qoiconv.c", "qoi.h" }

	filter { "configurations:Debug" }

	filter { "configurations:Release" }

	filter { }

project "qoibench"
	language "C++"
	kind "ConsoleApp"
	files { "qoibench.cpp", "qoi.h" }
	links { "libpng16", "zlib" }

	filter { "configurations:Debug" }

	filter { "configurations:Release" }

	filter { }
