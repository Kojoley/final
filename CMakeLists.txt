project(final)
cmake_minimum_required(VERSION 2.8)

# Include required CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(utils)

request_cxx11_compiler(TRUE)

include_directories(include)
include_directories(src/include)
aux_source_directory(src SRC_LIST_${PROJECT_NAME})
aux_source_directory(src/http SRC_LIST_${PROJECT_NAME})

# Hack for project file listing in Qt Creator
file(GLOB INCLUDE_LIST_${PROJECT_NAME} RELATIVE ${CMAKE_SOURCE_DIR} "include/*.h??")
list(APPEND SRC_LIST_${PROJECT_NAME} ${INCLUDE_LIST_${PROJECT_NAME}})

add_executable(${PROJECT_NAME} ${SRC_LIST_${PROJECT_NAME}})

enable_all_warnings(${PROJECT_NAME})

if(WIN32)
  # Determine and define _WIN32_WINNT
  init_winver()
  # Add windows related network library dependencies
  target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)
elseif(UNIX)
  # Boost.Application needs dl library
  target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})

  target_link_libraries(${PROJECT_NAME} -lrt)
endif()

setup_boost_settings()
# Force Boost::Application use of boost as default
# (boost::shared_ptr, boost::unordered_map and so on)
add_definitions(-DBOOST_APPLICATION_FEATURE_NS_SELECT_BOOST)
# Finally find Boost
find_package(Boost ${${CONFIG_VAR_PREFIX}_BOOST} COMPONENTS
    date_time regex chrono atomic
    system
    thread
    program_options
    filesystem
    log
    log_setup
    REQUIRED)
include_directories(BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})

# Boost.Fusion has broken constexpr support in Boost 1.58
# https://svn.boost.org/trac/boost/ticket/11211
if(Boost_VERSION VERSION_EQUAL 105800)
  message(WARNING "Because of Boost.Fusion constexpr support is disabled")
  add_definitions(-DBOOST_NO_CXX11_CONSTEXPR)
endif()
