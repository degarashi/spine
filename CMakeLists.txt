cmake_minimum_required(VERSION 2.8)
project(spine)
include(aux.cmake)

set(CXX_VERSION 17)
include(lubee/cxx_options.cmake)
string(CONCAT CMAKE_CXX_FLAGS " -Wno-c99-extensions")
string(CONCAT CMAKE_CXX_FLAGS " -Wno-gnu-zero-variadic-macro-arguments")

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT without-test)
	enable_testing()
	find_package(Threads REQUIRED)
	find_package(GTest REQUIRED)
	include_directories(${GTEST_INCLUDE_DIRS})
	# testsディレクトリ以下のソースが対象
	aux_source_directory(src/tests TEST_SRC)
	foreach(SRC IN LISTS TEST_SRC)
		GetFileName(${SRC}, SRCNAME)
		AddTest(${SRCNAME} ${SRC})
	endforeach()
endif()
