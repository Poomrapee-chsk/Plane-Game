# - Try to find Assimp
# Once done, this will define
#
# ASSIMP_FOUND - system has Assimp
# ASSIMP_INCLUDE_DIR - the Assimp include directories
# ASSIMP_LIBRARIES - link these to use Assimp
#
# You can specify ASSIMP_ROOT to use a custom installation:
# Set ASSIMP_ROOT environment variable or CMake variable to point to your Assimp installation

# Check environment for root search directory
SET(_assimp_ENV_ROOT $ENV{ASSIMP_ROOT})
IF(NOT ASSIMP_ROOT AND _assimp_ENV_ROOT)
	SET(ASSIMP_ROOT ${_assimp_ENV_ROOT})
ENDIF()

# Build search paths
SET(_assimp_HEADER_SEARCH_DIRS
	${CMAKE_SOURCE_DIR}/includes
)

SET(_assimp_LIB_SEARCH_DIRS
	${CMAKE_SOURCE_DIR}/lib
)

# Put user specified location at beginning of search
IF(ASSIMP_ROOT)
	LIST(INSERT _assimp_HEADER_SEARCH_DIRS 0 "${ASSIMP_ROOT}/include")
	LIST(INSERT _assimp_LIB_SEARCH_DIRS 0 "${ASSIMP_ROOT}/lib")
ENDIF()

FIND_PATH(ASSIMP_INCLUDE_DIR assimp/mesh.h
	PATHS ${_assimp_HEADER_SEARCH_DIRS}
	NO_DEFAULT_PATH
)

FIND_LIBRARY(ASSIMP_LIBRARY assimp
	PATHS ${_assimp_LIB_SEARCH_DIRS}
	NO_DEFAULT_PATH
)
IF(ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARY)
	SET( ASSIMP_FOUND TRUE )
	SET( ASSIMP_LIBRARIES ${ASSIMP_LIBRARY} )
ENDIF(ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARY)
IF(ASSIMP_FOUND)
	IF(NOT ASSIMP_FIND_QUIETLY)
	MESSAGE(STATUS "Found ASSIMP: ${ASSIMP_LIBRARY}")
	ENDIF(NOT ASSIMP_FIND_QUIETLY)
ELSE(ASSIMP_FOUND)
	IF(ASSIMP_FIND_REQUIRED)
	MESSAGE(FATAL_ERROR "Could not find libASSIMP")
	ENDIF(ASSIMP_FIND_REQUIRED)
ENDIF(ASSIMP_FOUND)
