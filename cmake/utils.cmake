macro(add_subdirectories)
  file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
  foreach(child ${children})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
      add_subdirectory(${child})
    endif()
  endforeach()
endmacro()

macro(init_winver)
  string(REGEX REPLACE
         "([0-9]).([0-9])" "0x\\10\\2"
         winver ${CMAKE_SYSTEM_VERSION})
  set(winver "0x0601")
  add_definitions("-D_WIN32_WINNT=${winver}")
endmacro()

macro(append_compile_flags target_name flags)
  set_property(TARGET ${target_name} APPEND_STRING
               PROPERTY COMPILE_FLAGS " ${flags}")
endmacro()

macro(append_link_flags target_name flags)
  set_property(TARGET ${target_name} APPEND_STRING
               PROPERTY LINK_FLAGS " ${flags}")
endmacro()

macro(enable_all_warnings target_name)
  # Enable compiler warnings
  if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    # /Wall is unusable, lots of really not even warnings (such as C4820 and etc)
    #append_compile_flags(${target_name} "/Wall")
    append_compile_flags(${target_name} "/W4")

    # disable silly warnings
    append_compile_flags(${target_name} "/wd4127") # conditional expression is constant

    # temporary disable some warnings (reenable after adding GCC -Wconversion -Wshadow)
    ## Features    Diagnostic Type    Default State    Level    Diagnostic Code    Diagnostic Message
    ## Variable Shadowing    Warning    On    W4    C4456    "declaration of '%$I' hides previous local declaration"
    ## Variable Shadowing    Warning    On    W4    C4457    "declaration of '%$I' hides function parameter"
    ## Variable Shadowing    Warning    On    W4    C4458    "declaration of '%$I' hides class member
    ## Variable Shadowing    Warning    On    W4    C4459    "declaration of '%$I' hides global declaration"
    append_compile_flags(${target_name} "/wd4244") # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    append_compile_flags(${target_name} "/wd4456") # declaration of 'y' hides previous local declaration
    append_compile_flags(${target_name} "/wd4459") # declaration of 'y' hides previous local declaration
    append_compile_flags(${target_name} "/wd4800") # 'type' : forcing value to bool 'true' or 'false' (performance warning)
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    append_compile_flags(${target_name} "-Weverything")
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    append_compile_flags(${target_name} "-Wall -Wextra -pedantic")
  endif()
endmacro()

macro(setup_boost_settings)
  # Setup Boost variables
  set(Boost_USE_MULTITHREADED ON)
  set(Boost_USE_STATIC_LIBS OFF)
  set(Boost_USE_STATIC_RUNTIME OFF)
  add_definitions(-DBOOST_ALL_DYN_LINK)
  # Enable showing linking boost libraries
  add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})
  # Enable thread-safe in Spirit (this affects only Spirit Classic)
  add_definitions(-DBOOST_SPIRIT_THREADSAFE)
  # Enable assertation handler
  #add_definitions(-DBOOST_ENABLE_ASSERT_HANDLER)
  # Switch from using ASIO as header library to static library
  #add_definitions(-DBOOST_ASIO_SEPARATE_COMPILATION)
  #add_library(libasio STATIC asio_impl.cpp)
  set(Boost_ADDITIONAL_VERSIONS "1.59.0" "1.59")
endmacro()


function(is_target_enabled target_name RESULT_NAME)
#  MESSAGE("CALLED is_target_enabled for ${CONFIG_VAR_PREFIX}_${target_name}")
  if(NOT DEFINED CONFIG_VAR_PREFIX)
    MESSAGE("INTERNAL VARIABLE CONFIG_VAR_PREFIX IS NOT DEFINED")
    MESSAGE("SEEM THAT BUILD STARTED NOT FROM ROOT PATH")
    MESSAGE("THIS IS OKAY ONLY IF YOU WANT IT")
    set(${RESULT_NAME} TRUE PARENT_SCOPE)
  else()
    set(IS_DO_BUILD_NAME ${CONFIG_VAR_PREFIX}_BUILD_${target_name})
    set("${IS_DO_BUILD_NAME}" TRUE CACHE BOOL "Enable ${target_name}")
#    MESSAGE("IS_DO_BUILD_NAME=${IS_DO_BUILD_NAME}")
    if(NOT DEFINED ${IS_DO_BUILD_NAME})
#      MESSAGE("NOT DEFINED ${CONFIG_VAR_PREFIX}_BUILD_${target_name}")
      set(${RESULT_NAME} FALSE PARENT_SCOPE)
    elseif(NOT ${IS_DO_BUILD_NAME})
#      MESSAGE("NOT ${IS_DO_BUILD_NAME} (==${${IS_DO_BUILD_NAME}})")
      set(${RESULT_NAME} FALSE PARENT_SCOPE)
    else()
#      MESSAGE("ELSE")
      set(${RESULT_NAME} TRUE PARENT_SCOPE)
    endif()
  endif()
#  MESSAGE("DELLAC is_target_enabled for ${CONFIG_VAR_PREFIX}_${target_name}")
endfunction()

macro(request_cxx11_compiler nofail)
  if(ARGN LESS 0)
    set(nofail FALSE)
  endif()
  # enable c++11
  if(MSVC)
    if(MSVC_VERSION LESS 1800)
      # https://msdn.microsoft.com/en-US/library/hh567368.aspx
      if(NOT nofail)
        message(FATAL_ERROR "At least Visual Studio 2015 is required for constexpr")
      endif()
    else()
      # do nothing as Visual Studio have enabled C++11 by default
      set(HAS_CXX11_COMPILER TRUE)
    endif()
  elseif(CMAKE_VERSION VERSION_LESS "3.1")
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU" OR
       "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR
       "${CMAKE_CXX_COMPILER_ID}" MATCHES "Intel")
      if(NOT ("${CMAKE_CXX_FLAGS}" MATCHES "-std=c\\+\\+0x" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=gnu\\+\\+0x" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=c\\+\\+11" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=gnu\\+\\+11" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=c\\+\\+14" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=c\\+\\+1y" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=gnu\\+\\+14" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=gnu\\+\\+1y" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=c\\+\\+1z" OR
              "${CMAKE_CXX_FLAGS}" MATCHES "-std=gnu\\+\\+1z"))
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
           CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7")
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
          set(HAS_CXX11_COMPILER TRUE)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel" AND
               CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
          # https://software.intel.com/en-us/articles/c0x-features-supported-by-intel-c-compiler
          if(NOT nofail)
            message(FATAL_ERROR "At least Intel® C++ Compiler 14 is required for constexpr")
          endif()
        else()
          set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
          set(HAS_CXX11_COMPILER TRUE)
        endif()
      else()
        set(HAS_CXX11_COMPILER TRUE)
      endif()
    else()
      message(WARNING "You compiler may be unsupported")
    endif()
  else()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(HAS_CXX11_COMPILER TRUE)
  endif()
endmacro()
