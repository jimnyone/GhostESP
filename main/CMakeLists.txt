file(GLOB_RECURSE app_sources "${CMAKE_SOURCE_DIR}/main/*.c")
file(GLOB_RECURSE axs15231b_sources "${CMAKE_SOURCE_DIR}/components/axs15231b/*.c")


list(APPEND app_sources ${axs15231b_sources})


set(required_components bt nvs_flash driver esp_http_server mdns json esp_http_client mbedtls fatfs sdmmc wpa_supplicant lvgl lvgl_esp32_drivers freertos esp_lcd)


set(sources ${app_sources})


if("${IDF_TARGET}" STREQUAL "esp32s3")
    # Add the M5GFX wrapper source file
    list(APPEND sources "vendor/m5/m5gfx_wrapper.cpp")
    # Add M5GFX to the required components
    list(APPEND required_components M5GFX)
endif()


<<<<<<< HEAD
idf_component_register(SRCS ${sources}
                       INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include"
                       REQUIRES ${required_components})
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-incompatible-pointer-types)
=======
# Add I80 display driver for TDisplay S3
if(idf_target STREQUAL "esp32s3" AND CONFIG_USE_TDISPLAY_S3)
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/vendor/i80_display.c")
endif()

# Only pull in IO expander manager when enabled so headers are exported (all targets)
if(CONFIG_USE_IO_EXPANDER)
    list(APPEND required_components io_manager)
endif()

if(CONFIG_ENABLE_NATIVE_SD_APPS)
    list(APPEND required_components elf_loader)
endif()

if(CONFIG_ENABLE_GHOSTSCRIPT)
    list(APPEND required_components lua)
    list(APPEND extra_includes "${CMAKE_SOURCE_DIR}/components/lua/src")
else()
    list(FILTER app_sources EXCLUDE REGEX "managers/ghostscript_.*\\.c")
    list(FILTER app_sources EXCLUDE REGEX "managers/views/ghostscript_.*\\.c")
    list(FILTER sources EXCLUDE REGEX "managers/ghostscript_.*\\.c")
    list(FILTER sources EXCLUDE REGEX "managers/views/ghostscript_.*\\.c")
endif()

# Add IEEE 802.15.4 component for Zigbee capture on ESP32-C5/C6
if(idf_target STREQUAL "esp32c5" OR idf_target STREQUAL "esp32c6")
    list(APPEND required_components ieee802154)
    # Add Zigbee manager source
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/managers/zigbee_manager.c")
endif()

# Add Ethernet component when Ethernet is enabled
if(CONFIG_WITH_ETHERNET)
    list(APPEND required_components esp_eth)
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/managers/ethernet_manager.c")
endif()

# Add BadUSB manager and HID script parser when BadUSB is enabled (S3 only)
if(CONFIG_HAS_BADUSB)
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/managers/badusb_manager.c")
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/managers/hid_script_parser.c")
    list(APPEND required_components esp_tinyusb)
endif()

# Add motion detector manager when camera is enabled (S3 only)
if(CONFIG_HAS_CAMERA)
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/managers/motion_detector_manager.c")
    list(APPEND sources "${CMAKE_SOURCE_DIR}/main/managers/camera_stream_manager.c")
    list(APPEND required_components esp32-camera)
    list(APPEND extra_includes "${CMAKE_SOURCE_DIR}/components/esp32-camera/driver/include")
    list(APPEND extra_includes "${CMAKE_SOURCE_DIR}/components/esp32-camera/conversions/include")
endif()

# Add TLV320DAC3100 driver component (code guarded by CONFIG_HAS_TLV320DAC_I2C)
list(APPEND required_components tlv320dac3100)

# Add audio codec component for audio receiver headers/libraries.
list(APPEND required_components esp_audio_codec)

set(embed_files_list "core/ouis.bin")

if(CONFIG_HAS_NFC)
    idf_component_register(SRCS ${sources}
                           INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" ${extra_includes}
                           REQUIRES ${required_components}
                           EMBED_TXTFILES "mf_classic_dict.nfc"
                           EMBED_FILES ${embed_files_list})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_MFC_DICT_EMBEDDED=1)
else()
    idf_component_register(SRCS ${sources}
                           INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include" ${extra_includes}
                           REQUIRES ${required_components}
                           EMBED_FILES ${embed_files_list})
endif()

# add compile definition for esp32s2 target to maintain legacy guards (needs COMPONENT_LIB)
if(idf_target STREQUAL "esp32s2")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_IDF_TARGET_ESP32S2)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-incompatible-pointer-types -Wno-unused-local-typedefs)

idf_build_get_property(idf_path IDF_PATH)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${idf_path}/components/wpa_supplicant/src"
    "${idf_path}/components/wpa_supplicant/esp_supplicant/src"
)
>>>>>>> 73ca60d6 (Merge branch 'scripts' into pr/338)
