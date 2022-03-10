#
# This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# output generic information about the core and buildtype chosen
message("")

message("* WarheadDiscord revision  : ${rev_hash} ${rev_date} (${rev_branch} branch)")

if( UNIX )
  message("* WarheadDiscord buildtype     : ${CMAKE_BUILD_TYPE}")
  message("")
  message("* Install to               : ${CMAKE_INSTALL_PREFIX}")
  message("* Install libraries to     : ${LIBSDIR}")
  message("* Install configs to       : ${CONF_DIR}")
endif()

add_definitions(-D_CONF_DIR=$<1:"${CONF_DIR}">)

if (WITH_WARNINGS)
  message("* Show all warnings        : Yes")
else()
  message("* Show compile-warnings    : No  (default)")
endif()

if (WIN32)
  if(NOT WITH_SOURCE_TREE STREQUAL "no")
    message("* Show source tree         : Yes - \"${WITH_SOURCE_TREE}\"")
  else()
    message("* Show source tree         : No")
  endif()
else()
  message("* Show source tree           : No (For UNIX default)")
endif()

if (BUILD_SHARED_LIBS)
  message("")
  message(" *** WITH_DYNAMIC_LINKING - INFO!")
  message(" *** Will link against shared libraries!")
  message(" *** Please note that this is an experimental feature!")
  add_definitions(-DWARHEAD_API_USE_DYNAMIC_LINKING)
endif()

message("")
