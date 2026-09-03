function(sd_find_sdl3)
    if(TARGET SDL3::SDL3)
        return()
    endif()

    if(DEFINED SDL3_DIR AND SDL3_DIR AND NOT SDL3_DIR MATCHES "-NOTFOUND$")
        find_package(SDL3 3.4.14 CONFIG QUIET)
        if(SDL3_FOUND)
            message(STATUS "Using SDL3 ${SDL3_VERSION}: ${SDL3_DIR}")
            return()
        endif()
    endif()

    set(sdl3_local_prefix "${SD_PROJECT_ROOT}/ThirdParty/SDL/installed")
    find_package(SDL3 3.4.14 CONFIG QUIET
        PATHS "${sdl3_local_prefix}"
        NO_DEFAULT_PATH
    )
    if(SDL3_FOUND)
        message(STATUS "Using bundled SDL3 ${SDL3_VERSION}: ${sdl3_local_prefix}")
        return()
    endif()

    find_package(SDL3 3.4.14 CONFIG QUIET)
    if(SDL3_FOUND)
        message(STATUS "Using system SDL3 ${SDL3_VERSION}")
    endif()
endfunction()
