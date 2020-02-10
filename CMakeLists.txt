cmake_minimum_required( VERSION 2.8.4 )
project( textFeats )
set( tool_EXE textFeats )
find_package( PkgConfig )
#find_package( HDF5 REQUIRED )
pkg_check_modules( Magick REQUIRED Magick++>=6.8.9 )
pkg_check_modules( libxml REQUIRED libxml-2.0>=2.9 )
pkg_check_modules( libxslt REQUIRED libxslt )
pkg_check_modules( libcfg REQUIRED libconfig++ )
#pkg_check_modules( hdf5 hdf5-serial )
pkg_check_modules( opencv opencv4 )
if( NOT opencv_CFLAGS )
  pkg_check_modules( opencv REQUIRED opencv>=2.4 )
endif()

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

#find_package( OpenMP )
#if( OPENMP_FOUND )
#  #set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}" )
#  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" )
#endif()

set( CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES};${opencv_INCLUDE_DIRS}" )

add_definitions( -D__PAGEXML_MAGICK__ )
add_definitions( -D__PAGEXML_IMG_MAGICK__ )
#add_definitions( -D__PAGEXML_IMG_CV__ )
add_definitions( -D__PAGEXML_LIBCONFIG__ )

file( GLOB tool_SRC "*.cc" )
add_executable( ${tool_EXE} ${tool_SRC} )
set_property( TARGET ${tool_EXE} PROPERTY CXX_STANDARD 11 )

string( REPLACE ";" " " CFLAGS_STR "-Wall -W ${Magick_CFLAGS} ${opencv_CFLAGS} ${libxml_CFLAGS} ${libxslt_CFLAGS} ${hdf5_CFLAGS}" )
set_target_properties( ${tool_EXE} PROPERTIES COMPILE_FLAGS "${CFLAGS_STR}" )

include_directories( SYSTEM ${Magick_INCLUDEDIR} ) # To suppress system header warnings

#include_directories( ${HDF5_INCLUDE_DIRS} )

#target_link_libraries( ${tool_EXE} ${Magick_LDFLAGS} ${opencv_LDFLAGS} ${libxml_LDFLAGS} ${libxslt_LDFLAGS} ${libcfg_LDFLAGS} ${HDF5_LIBRARIES} ${hdf5_LDFLAGS} hdf5_cpp pthread jpeg tiff png m )
target_link_libraries( ${tool_EXE} ${Magick_LDFLAGS} ${opencv_LDFLAGS} ${libxml_LDFLAGS} ${libxslt_LDFLAGS} ${libcfg_LDFLAGS} pthread jpeg tiff png m )

enable_testing()
add_test( NAME raw_xml_extraction
          COMMAND ${CMAKE_COMMAND}
          -DTEST_PROG=$<TARGET_FILE:${tool_EXE}>
          -DSOURCEDIR=${CMAKE_CURRENT_SOURCE_DIR}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/test/raw_xml_extraction.cmake )

install( TARGETS ${tool_EXE} DESTINATION bin )
add_custom_target( install-docker cp ${CMAKE_HOME_DIRECTORY}/textFeats-docker ${CMAKE_INSTALL_PREFIX}/bin )

add_custom_target( realclean cd ${CMAKE_HOME_DIRECTORY} COMMAND rm -fr ${tool_EXE} ${tool_EXE}.exe ${tool_EXE}.dSYM CMakeFiles CMakeCache.txt cmake_install.cmake install_manifest.txt Makefile CTestTestfile.cmake Testing )
