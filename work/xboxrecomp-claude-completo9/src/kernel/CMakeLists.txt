cmake_minimum_required(VERSION 3.20)

# Xbox Kernel Replacement Layer
# Replaces 147 Xbox kernel imports with Win32 equivalents.
# Memory layout, file I/O, threading, synchronization, crypto, HAL.

set(KERNEL_SOURCES
    kernel_path.c
    kernel_pool.c
    kernel_rtl.c
    kernel_memory.c
    kernel_file.c
    kernel_missing.c
    kernel_thread.c
    kernel_sync.c
    kernel_hal.c
    kernel_guest_cpu.c
    kernel_xbox.c
    kernel_ob.c
    kernel_io.c
    kernel_crypto.c
    kernel_thunks.c
    kernel_bridge.c
    xbox_memory_layout.c
    guest_vmem.c
    icall_feedback.c
    recomp_trace.c
    nv2a_pb_scan.c
    nv2a_pb_exec.c
    nv2a_flip_hold.c
    nv2a_vsh_interp.c
    nv2a_combiner.c
)

# Direct3D 11 renderer from The Two Towers Recompiled (MIT; gpu_tt/, see
# nv2a_gpu_tt.c). Built with clang/MinGW there: x86intrin.h stand-in and
# quieter warnings for its files under MSVC.
if(WIN32)
    list(APPEND KERNEL_SOURCES nv2a_gpu_tt.c nv2a_gpu_tt_shims.c)
    # The renderer is one translation unit containing .inc files. Explicit
    # dependencies also work when localized MSVC /showIncludes is not parsed.
    file(GLOB_RECURSE GPU_TT_INCLUDES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tt/*.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tt/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tt/*.c")
    set_property(SOURCE nv2a_gpu_tt.c APPEND PROPERTY OBJECT_DEPENDS ${GPU_TT_INCLUDES})
    if(MSVC)
        set_source_files_properties(nv2a_gpu_tt.c PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tt/compat"
            COMPILE_OPTIONS "/W1")
    endif()
endif()

add_library(xbox_kernel STATIC ${KERNEL_SOURCES})

# See src/platform/CMakeLists.txt: resolve src/ relative to this file, not via
# CMAKE_SOURCE_DIR, which is the consuming project's root under add_subdirectory().
get_filename_component(XBOXRECOMP_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

target_include_directories(xbox_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XBOXRECOMP_SRC_DIR}
)

if(WIN32)
    # platform is an INTERFACE target on Windows: it contributes the src/
    # include path that "platform/xbox_winnt.h" resolves against.
    target_link_libraries(xbox_kernel PUBLIC platform kernel32 bcrypt dxguid)
else()
    # Linux: platform layer (POSIX host primitives) + OpenSSL for crypto.
    find_package(OpenSSL REQUIRED)
    target_link_libraries(xbox_kernel PUBLIC platform OpenSSL::Crypto)
endif()

if(MSVC)
    target_compile_options(xbox_kernel PRIVATE /W4 /WX- /O2 /GS- /Gy)
    target_compile_definitions(xbox_kernel PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(xbox_kernel PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
endif()

# The trapped-MMIO dispatch (xbox_VehMmioFault, xbox_PosixMmioFault) routes
# to the APU and the USB OHCI models. Link-only: both already call back into
# the kernel, and CMake repeats static libraries in such a cycle.
target_link_libraries(xbox_kernel PRIVATE xbox_apu xbox_usb)

target_compile_definitions(xbox_kernel PRIVATE $<$<CONFIG:Debug>:_DEBUG>)
