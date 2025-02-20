#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "NebulaMapper::nebula_mapper_lib" for configuration "Debug"
set_property(TARGET NebulaMapper::nebula_mapper_lib APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(NebulaMapper::nebula_mapper_lib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libnebula_mapper_lib.a"
  )

list(APPEND _cmake_import_check_targets NebulaMapper::nebula_mapper_lib )
list(APPEND _cmake_import_check_files_for_NebulaMapper::nebula_mapper_lib "${_IMPORT_PREFIX}/lib/libnebula_mapper_lib.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
