cmake_minimum_required(VERSION 2.8)
project(spine)

set(COMMON_COMPILE lubee/common_compile)
include(${COMMON_COMPILE}/cxx_setup.cmake)
include(${COMMON_COMPILE}/compile_commands.cmake)
include(${COMMON_COMPILE}/aux.cmake)
SetupCXX(17)
string(CONCAT CMAKE_CXX_FLAGS " -Wno-c99-extensions")
string(CONCAT CMAKE_CXX_FLAGS " -Wno-gnu-zero-variadic-macro-arguments")

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT without-test)
	enable_testing()
	# testsディレクトリ以下のソースが対象
	aux_source_directory(src/tests TEST_SRC)
	LoadGTestLibs(LIBS)
	foreach(SRC IN LISTS TEST_SRC)
		GetFileName(${SRC}, SRCNAME)
		AddTest(${PROJECT_NAME} ${SRCNAME} ${SRC} "${LIBS}")
	endforeach()
	DefineCompDB(TEST_SRC)
endif()
