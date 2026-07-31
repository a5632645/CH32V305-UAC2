#############################################################[Chinese Manual]###################################################################
# *****工程路径不要包含中文字符和空格,否则可能会报错！*****
# 第一步： 确认机器上已经安装了MRS的工具链(假设当前平台为Linux,工具链路径为"/path/to/toolchain/RISC-V Embedded GCC12",工具链相关前缀为"riscv-wch-elf-")
# 第二步: 设置工具链路径,例如修改替换下方的set(TOOLCHAIN_FOLDER "/path/to/toolchain/RISC-V Embedded GCC12")
# 第三步: 设置工具链中相关可执行文件的前缀,例如修改替换下方的set(TOOLCHAIN_PREFIX "riscv-wch-elf-")
# 第四步: 执行 cd your/path/of/project
# 第五步: 执行 cmake -B build -G "Unix Makefiles"
# 第六步: 执行 cmake --build build
# 注意: 在Windows平台运行,需要a.安装GNU MAKE并将其路径(bin目录)添加到系统环境变量,之后执行cmake -B build -G "Unix Makefiles"(推荐)
#       或者b.安装MinGW,将其路径(bin目录)添加到系统环境变量,之后执行cmake -B build -G "MinGW Makefiles"
# 如有相关疑问,可通过support@mounriver.com联系我们
#################################################################################################################################################


#############################################################[English Manual]###################################################################
# Step 1: Confirm that the MRS toolchain has been installed on the machine. Assuming the current platform is Linux, 
#         the toolchain path is "/path/to/toolchain/RISC-V Embedded GCC12", and the toolchain-related prefix is "riscv-wch-elf-"
# Step 2: Set the toolchain path, for example, modify and replace the following "set (TOOLCHAIN_FOLDER "/path/to/toolchain/RISC-V Embedded GCC12")"
# Step 3: Set the prefix of the executable files in the toolchain,for example, modify and replace the following "set(TOOLCHAIN_PREFIX "riscv-wch-elf-")"
# Step 4: Call "cd your/path/of/project"
# Step 5: Call "cmake -B build -G "Unix Makefiles""
# Step 6: Call "cmake --build build"
# Note:   To run on Windows, you need: a. Install GNU MAKE and add its path (the bin directory) to the system environment PATH. 
#         Then call the execution of "cmake -B build -G "Unix Makefiles""(Recommanded).
#         or b. Install MinGW and add its path (the bin directory) to the system environment PATH. 
#         Then call the execution of "cmake -B build -G "MinGW Makefiles"".
# If you have any questions, please contact us at support@mounriver.com
#################################################################################################################################################


# Set CMake minimum require
cmake_minimum_required(VERSION 3.16)

# Set CMake sysytem name
set(CMAKE_SYSTEM_NAME Generic)


# Skipping compiler detection and use custom toolchain
SET(CMAKE_C_COMPILER_WORKS TRUE)
SET(CMAKE_CXX_COMPILER_WORKS TRUE)
SET(CMAKE_ASM_COMPILER_WORKS TRUE)

# Use MRS toolchian to compile
set(TOOLCHAIN_FOLDER "c:/MounRiver/MounRiver_Studio2/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC")
set(TOOLCHAIN_PREFIX "riscv-none-embed-")
set(CMAKE_C_COMPILER "${TOOLCHAIN_FOLDER}/bin/riscv-none-embed-gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_FOLDER}/bin/riscv-none-embed-g++.exe")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_FOLDER}/bin/riscv-none-embed-gcc.exe")
set(CMAKE_OBJCOPY "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}objcopy.exe")
set(CMAKE_OBJDUMP "${TOOLCHAIN_FOLDER}/bin/riscv-none-embed-objdump.exe")
set(CMAKE_SIZE "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}size.exe")
set(CMAKE_AR "${TOOLCHAIN_FOLDER}/bin/${TOOLCHAIN_PREFIX}ar.exe")

set(PROJECT_NAME "HSDAC")
set(TARGET_ARTIFACT "HSDAC.elf")

set(CMAKE_C_OUTPUT_EXTENSION .o)
set(CMAKE_CXX_OUTPUT_EXTENSION .o)

project("${PROJECT_NAME}" LANGUAGES C ASM CXX)

# Use relative path
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=..)


set(SRC_FILES 
"${CMAKE_CURRENT_SOURCE_DIR}/env/core_riscv.c"
"${CMAKE_CURRENT_SOURCE_DIR}/env/startup_ch32v30x_D8C.S"
"${CMAKE_CURRENT_SOURCE_DIR}/User/usb/usb_desc.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/User/usb/usb_device.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/usb/usb_impl.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/audio_dsp.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/ch32v30x_it.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/codec.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/codec_i2c.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/codec_i2s.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/debug.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/uac_param.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/main.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/system_ch32v30x.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/tick.c"
"${CMAKE_CURRENT_SOURCE_DIR}/User/usbd.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_adc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_bkp.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_can.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_crc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_dac.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_dbgmcu.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_dma.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_dvp.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_eth.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_exti.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_flash.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_fsmc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_gpio.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_i2c.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_iwdg.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_misc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_opa.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_pwr.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_rcc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_rng.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_rtc.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_sdio.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_spi.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_tim.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_usart.c"
"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src/ch32v30x_wwdg.c"
)
set(CMAKE_C_FLAGS " -march=rv32imafcxw -mabi=ilp32f -msmall-data-limit=8 -msave-restore -fmax-errors=20 -O2 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-inline-functions -Wunused -Wuninitialized -Wall -Wextra -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/env\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/inc\" -std=gnu11")
set(CMAKE_CXX_FLAGS " -march=rv32imafcxw -mabi=ilp32f -msmall-data-limit=8 -msave-restore -fmax-errors=20 -O2 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-inline-functions -Wunused -Wuninitialized -Wall -Wextra -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/env\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/inc\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/User\" -std=gnu++17 -fabi-version=0")
set(CMAKE_ASM_FLAGS " -march=rv32imafcxw -mabi=ilp32f -msmall-data-limit=8 -msave-restore -fmax-errors=20 -O2 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-inline-functions -Wunused -Wuninitialized -Wall -Wextra -g -I\"${CMAKE_CURRENT_SOURCE_DIR}/env\" -x assembler-with-cpp")

add_executable("${TARGET_ARTIFACT}" ${SRC_FILES})

set_target_properties("${TARGET_ARTIFACT}" PROPERTIES LINK_FLAGS " -march=rv32imafcxw -mabi=ilp32f -msmall-data-limit=8 -msave-restore -fmax-errors=20 -O2 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-inline-functions -Wunused -Wuninitialized -Wall -Wextra -g -T \"${CMAKE_CURRENT_SOURCE_DIR}/env/Link.ld\"   -nostartfiles -Xlinker --gc-sections -Wl,-Map,\"HSDAC.map\" --specs=nano.specs --specs=nosys.specs")

target_link_libraries(
"${TARGET_ARTIFACT}" 
m
)

add_custom_command(TARGET "${TARGET_ARTIFACT}" 
POST_BUILD 
COMMAND ${CMAKE_OBJCOPY} ARGS -O binary  "HSDAC.elf" "HSDAC.bin"
COMMAND ${CMAKE_OBJCOPY} ARGS -O ihex  "HSDAC.elf" "HSDAC.hex"
COMMAND ${CMAKE_OBJDUMP} ARGS  --all-headers --demangle --disassemble -M xw "HSDAC.elf" > "HSDAC.lst"
COMMAND ${CMAKE_SIZE} ARGS  --format=berkeley "HSDAC.elf"
)
