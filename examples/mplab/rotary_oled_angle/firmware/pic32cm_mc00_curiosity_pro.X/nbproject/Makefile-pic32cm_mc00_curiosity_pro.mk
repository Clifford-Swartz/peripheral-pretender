#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Include project Makefile
ifeq "${IGNORE_LOCAL}" "TRUE"
# do not include local makefile. User is passing all local related variables already
else
include Makefile
# Include makefile containing local settings
ifeq "$(wildcard nbproject/Makefile-local-pic32cm_mc00_curiosity_pro.mk)" "nbproject/Makefile-local-pic32cm_mc00_curiosity_pro.mk"
include nbproject/Makefile-local-pic32cm_mc00_curiosity_pro.mk
endif
endif

# Environment
MKDIR=gnumkdir -p
RM=rm -f 
MV=mv 
CP=cp 

# Macros
CND_CONF=pic32cm_mc00_curiosity_pro
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
IMAGE_TYPE=debug
OUTPUT_SUFFIX=elf
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=elf
FINAL_IMAGE=${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
endif

ifeq ($(COMPARE_BUILD), true)
COMPARISON_BUILD=-mafrlcsj
else
COMPARISON_BUILD=
endif

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Source Files Quoted if spaced
SOURCEFILES_QUOTED_IF_SPACED=../src/config/pic32cm_mc00_curiosity_pro/peripheral/clock/plib_clock.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/evsys/plib_evsys.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvic/plib_nvic.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/port/plib_port.c ../src/config/pic32cm_mc00_curiosity_pro/stdio/xc32_monitor.c ../src/config/pic32cm_mc00_curiosity_pro/initialization.c ../src/config/pic32cm_mc00_curiosity_pro/interrupts.c ../src/config/pic32cm_mc00_curiosity_pro/exceptions.c ../src/config/pic32cm_mc00_curiosity_pro/startup_xc32.c ../src/config/pic32cm_mc00_curiosity_pro/libc_syscalls.c ../src/main.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/_ext/676635429/plib_clock.o ${OBJECTDIR}/_ext/678784915/plib_evsys.o ${OBJECTDIR}/_ext/160711403/plib_nvic.o ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o ${OBJECTDIR}/_ext/160764554/plib_port.o ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o ${OBJECTDIR}/_ext/1997885285/initialization.o ${OBJECTDIR}/_ext/1997885285/interrupts.o ${OBJECTDIR}/_ext/1997885285/exceptions.o ${OBJECTDIR}/_ext/1997885285/startup_xc32.o ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o ${OBJECTDIR}/_ext/1360937237/main.o
POSSIBLE_DEPFILES=${OBJECTDIR}/_ext/676635429/plib_clock.o.d ${OBJECTDIR}/_ext/678784915/plib_evsys.o.d ${OBJECTDIR}/_ext/160711403/plib_nvic.o.d ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o.d ${OBJECTDIR}/_ext/160764554/plib_port.o.d ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o.d ${OBJECTDIR}/_ext/1997885285/initialization.o.d ${OBJECTDIR}/_ext/1997885285/interrupts.o.d ${OBJECTDIR}/_ext/1997885285/exceptions.o.d ${OBJECTDIR}/_ext/1997885285/startup_xc32.o.d ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o.d ${OBJECTDIR}/_ext/1360937237/main.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/_ext/676635429/plib_clock.o ${OBJECTDIR}/_ext/678784915/plib_evsys.o ${OBJECTDIR}/_ext/160711403/plib_nvic.o ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o ${OBJECTDIR}/_ext/160764554/plib_port.o ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o ${OBJECTDIR}/_ext/1997885285/initialization.o ${OBJECTDIR}/_ext/1997885285/interrupts.o ${OBJECTDIR}/_ext/1997885285/exceptions.o ${OBJECTDIR}/_ext/1997885285/startup_xc32.o ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o ${OBJECTDIR}/_ext/1360937237/main.o

# Source Files
SOURCEFILES=../src/config/pic32cm_mc00_curiosity_pro/peripheral/clock/plib_clock.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/evsys/plib_evsys.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvic/plib_nvic.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c ../src/config/pic32cm_mc00_curiosity_pro/peripheral/port/plib_port.c ../src/config/pic32cm_mc00_curiosity_pro/stdio/xc32_monitor.c ../src/config/pic32cm_mc00_curiosity_pro/initialization.c ../src/config/pic32cm_mc00_curiosity_pro/interrupts.c ../src/config/pic32cm_mc00_curiosity_pro/exceptions.c ../src/config/pic32cm_mc00_curiosity_pro/startup_xc32.c ../src/config/pic32cm_mc00_curiosity_pro/libc_syscalls.c ../src/main.c



CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
# fixDeps replaces a bunch of sed/cat/printf statements that slow down the build
FIXDEPS=fixDeps

.build-conf:  ${BUILD_SUBPROJECTS}
ifneq ($(INFORMATION_MESSAGE), )
	@echo $(INFORMATION_MESSAGE)
endif
	${MAKE}  -f nbproject/Makefile-pic32cm_mc00_curiosity_pro.mk ${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

MP_PROCESSOR_OPTION=32CM1216MC00048
MP_LINKER_FILE_OPTION=,--script="..\src\config\pic32cm_mc00_curiosity_pro\PIC32CM1216MC00048.ld"
# ------------------------------------------------------------------------------------
# Rules for buildStep: assemble
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: assembleWithPreprocess
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compile
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/_ext/676635429/plib_clock.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/clock/plib_clock.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/991aaa763e983be8cb234506c1335f3f3bda9e5b .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/676635429" 
	@${RM} ${OBJECTDIR}/_ext/676635429/plib_clock.o.d 
	@${RM} ${OBJECTDIR}/_ext/676635429/plib_clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/676635429/plib_clock.o.d" -o ${OBJECTDIR}/_ext/676635429/plib_clock.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/clock/plib_clock.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/678784915/plib_evsys.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/evsys/plib_evsys.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/1dc04a9accb353df8aa8b928c57e6d38ccd33356 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/678784915" 
	@${RM} ${OBJECTDIR}/_ext/678784915/plib_evsys.o.d 
	@${RM} ${OBJECTDIR}/_ext/678784915/plib_evsys.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/678784915/plib_evsys.o.d" -o ${OBJECTDIR}/_ext/678784915/plib_evsys.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/evsys/plib_evsys.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/160711403/plib_nvic.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvic/plib_nvic.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/f1806c5c092bd8362b7b652454ed53bae907f250 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/160711403" 
	@${RM} ${OBJECTDIR}/_ext/160711403/plib_nvic.o.d 
	@${RM} ${OBJECTDIR}/_ext/160711403/plib_nvic.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/160711403/plib_nvic.o.d" -o ${OBJECTDIR}/_ext/160711403/plib_nvic.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvic/plib_nvic.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/25bf743b0abffe40f875f156d08f8ab2b7aea3db .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1131319065" 
	@${RM} ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o.d 
	@${RM} ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o.d" -o ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/160764554/plib_port.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/port/plib_port.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/c6da764d2397ebd6208d1a504bca22945c2e6e08 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/160764554" 
	@${RM} ${OBJECTDIR}/_ext/160764554/plib_port.o.d 
	@${RM} ${OBJECTDIR}/_ext/160764554/plib_port.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/160764554/plib_port.o.d" -o ${OBJECTDIR}/_ext/160764554/plib_port.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/port/plib_port.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1429341023/xc32_monitor.o: ../src/config/pic32cm_mc00_curiosity_pro/stdio/xc32_monitor.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/d6d4d33f58dbcb6c25dde4ad82c425eb3019109c .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1429341023" 
	@${RM} ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o.d 
	@${RM} ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1429341023/xc32_monitor.o.d" -o ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o ../src/config/pic32cm_mc00_curiosity_pro/stdio/xc32_monitor.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/initialization.o: ../src/config/pic32cm_mc00_curiosity_pro/initialization.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/10bd7fb8a678eeb6b8317cd9ca040691ba951c4f .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/initialization.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/initialization.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/initialization.o.d" -o ${OBJECTDIR}/_ext/1997885285/initialization.o ../src/config/pic32cm_mc00_curiosity_pro/initialization.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/interrupts.o: ../src/config/pic32cm_mc00_curiosity_pro/interrupts.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/9dcb3678849cbc320345428ede23d25997295340 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/interrupts.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/interrupts.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/interrupts.o.d" -o ${OBJECTDIR}/_ext/1997885285/interrupts.o ../src/config/pic32cm_mc00_curiosity_pro/interrupts.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/exceptions.o: ../src/config/pic32cm_mc00_curiosity_pro/exceptions.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/ed597ec0631044a051d2991361137efab1cf7cd3 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/exceptions.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/exceptions.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/exceptions.o.d" -o ${OBJECTDIR}/_ext/1997885285/exceptions.o ../src/config/pic32cm_mc00_curiosity_pro/exceptions.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/startup_xc32.o: ../src/config/pic32cm_mc00_curiosity_pro/startup_xc32.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/7d3baf311447172ebb91357845be65713dad7f3a .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/startup_xc32.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/startup_xc32.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/startup_xc32.o.d" -o ${OBJECTDIR}/_ext/1997885285/startup_xc32.o ../src/config/pic32cm_mc00_curiosity_pro/startup_xc32.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/libc_syscalls.o: ../src/config/pic32cm_mc00_curiosity_pro/libc_syscalls.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/94fc98013d382440828734db98f85cec251b8976 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/libc_syscalls.o.d" -o ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o ../src/config/pic32cm_mc00_curiosity_pro/libc_syscalls.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1360937237/main.o: ../src/main.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/b1bc9db01abd2a839c1d0cd47890967b582ee1e1 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE) -g -D__DEBUG   -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1360937237/main.o.d" -o ${OBJECTDIR}/_ext/1360937237/main.o ../src/main.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
else
${OBJECTDIR}/_ext/676635429/plib_clock.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/clock/plib_clock.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/823ff038b20cc7e09dab29dc4da79d28e2dbb072 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/676635429" 
	@${RM} ${OBJECTDIR}/_ext/676635429/plib_clock.o.d 
	@${RM} ${OBJECTDIR}/_ext/676635429/plib_clock.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/676635429/plib_clock.o.d" -o ${OBJECTDIR}/_ext/676635429/plib_clock.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/clock/plib_clock.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/678784915/plib_evsys.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/evsys/plib_evsys.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/fd935d9402fd597c005cac099c77111f0f2d11fb .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/678784915" 
	@${RM} ${OBJECTDIR}/_ext/678784915/plib_evsys.o.d 
	@${RM} ${OBJECTDIR}/_ext/678784915/plib_evsys.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/678784915/plib_evsys.o.d" -o ${OBJECTDIR}/_ext/678784915/plib_evsys.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/evsys/plib_evsys.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/160711403/plib_nvic.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvic/plib_nvic.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/88f3dd21a287abf6ffdc885acfbea29e09bd74ce .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/160711403" 
	@${RM} ${OBJECTDIR}/_ext/160711403/plib_nvic.o.d 
	@${RM} ${OBJECTDIR}/_ext/160711403/plib_nvic.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/160711403/plib_nvic.o.d" -o ${OBJECTDIR}/_ext/160711403/plib_nvic.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvic/plib_nvic.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/c9cbed85dc5cb56354e6cdd41e0e107f5aefa19e .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1131319065" 
	@${RM} ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o.d 
	@${RM} ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o.d" -o ${OBJECTDIR}/_ext/1131319065/plib_nvmctrl.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/160764554/plib_port.o: ../src/config/pic32cm_mc00_curiosity_pro/peripheral/port/plib_port.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/4c03933de7a165639d2c75156d0f4151839f01f4 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/160764554" 
	@${RM} ${OBJECTDIR}/_ext/160764554/plib_port.o.d 
	@${RM} ${OBJECTDIR}/_ext/160764554/plib_port.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/160764554/plib_port.o.d" -o ${OBJECTDIR}/_ext/160764554/plib_port.o ../src/config/pic32cm_mc00_curiosity_pro/peripheral/port/plib_port.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1429341023/xc32_monitor.o: ../src/config/pic32cm_mc00_curiosity_pro/stdio/xc32_monitor.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/67a7bb8c61094dee5ec7fdad40a46b37e80792f3 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1429341023" 
	@${RM} ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o.d 
	@${RM} ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1429341023/xc32_monitor.o.d" -o ${OBJECTDIR}/_ext/1429341023/xc32_monitor.o ../src/config/pic32cm_mc00_curiosity_pro/stdio/xc32_monitor.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/initialization.o: ../src/config/pic32cm_mc00_curiosity_pro/initialization.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/688184b5557466ba6a7d4460e833132551a8b786 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/initialization.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/initialization.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/initialization.o.d" -o ${OBJECTDIR}/_ext/1997885285/initialization.o ../src/config/pic32cm_mc00_curiosity_pro/initialization.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/interrupts.o: ../src/config/pic32cm_mc00_curiosity_pro/interrupts.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/cd01fa8c3e33dcf11a6e0b1360eda05d15e7c16a .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/interrupts.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/interrupts.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/interrupts.o.d" -o ${OBJECTDIR}/_ext/1997885285/interrupts.o ../src/config/pic32cm_mc00_curiosity_pro/interrupts.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/exceptions.o: ../src/config/pic32cm_mc00_curiosity_pro/exceptions.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/60e992d3fa8e15eeff2712f70a6884886df81336 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/exceptions.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/exceptions.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/exceptions.o.d" -o ${OBJECTDIR}/_ext/1997885285/exceptions.o ../src/config/pic32cm_mc00_curiosity_pro/exceptions.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/startup_xc32.o: ../src/config/pic32cm_mc00_curiosity_pro/startup_xc32.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/80b7daac3ebb924915f8d14ad457b9ae244ab87a .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/startup_xc32.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/startup_xc32.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/startup_xc32.o.d" -o ${OBJECTDIR}/_ext/1997885285/startup_xc32.o ../src/config/pic32cm_mc00_curiosity_pro/startup_xc32.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1997885285/libc_syscalls.o: ../src/config/pic32cm_mc00_curiosity_pro/libc_syscalls.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/48a8475fb21a25f89b4add70ee7b7e911ee0c82d .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1997885285" 
	@${RM} ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o.d 
	@${RM} ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1997885285/libc_syscalls.o.d" -o ${OBJECTDIR}/_ext/1997885285/libc_syscalls.o ../src/config/pic32cm_mc00_curiosity_pro/libc_syscalls.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
${OBJECTDIR}/_ext/1360937237/main.o: ../src/main.c  .generated_files/flags/pic32cm_mc00_curiosity_pro/e5317d0bbb079ec35257abf3890c8378b658ae03 .generated_files/flags/pic32cm_mc00_curiosity_pro/da39a3ee5e6b4b0d3255bfef95601890afd80709
	@${MKDIR} "${OBJECTDIR}/_ext/1360937237" 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o.d 
	@${RM} ${OBJECTDIR}/_ext/1360937237/main.o 
	${MP_CC}  $(MP_EXTRA_CC_PRE)  -g -x c -c -mprocessor=$(MP_PROCESSOR_OPTION)  -ffunction-sections -fdata-sections -O1 -fno-common -I"../src" -I"../src/config/pic32cm_mc00_curiosity_pro" -I"../src/packs/CMSIS/" -I"../src/packs/CMSIS/CMSIS/Core/Include" -I"../src/packs/PIC32CM1216MC00048_DFP" -MP -MMD -MF "${OBJECTDIR}/_ext/1360937237/main.o.d" -o ${OBJECTDIR}/_ext/1360937237/main.o ../src/main.c    -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wformat=2 -Werror-implicit-function-declaration -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wmissing-noreturn -mdfp="${DFP_DIR}/PIC32CM1216MC00048" ${PACK_COMMON_OPTIONS} 
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compileCPP
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    ../src/config/pic32cm_mc00_curiosity_pro/PIC32CM1216MC00048.ld
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE) -g   -mprocessor=$(MP_PROCESSOR_OPTION)  -mno-device-startup-code -o ${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=__ICD2RAM=1,--defsym=__MPLAB_DEBUG=1,--defsym=__DEBUG=1,-D=__DEBUG_D,--defsym=_min_heap_size=512,--gc-sections,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}/PIC32CM1216MC00048"
	
else
${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   ../src/config/pic32cm_mc00_curiosity_pro/PIC32CM1216MC00048.ld
	@${MKDIR} ${DISTDIR} 
	${MP_CC} $(MP_EXTRA_LD_PRE)  -mprocessor=$(MP_PROCESSOR_OPTION)  -mno-device-startup-code -o ${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} ${OBJECTFILES_QUOTED_IF_SPACED}          -DXPRJ_pic32cm_mc00_curiosity_pro=$(CND_CONF)    $(COMPARISON_BUILD)  -Wl,--defsym=__MPLAB_BUILD=1$(MP_EXTRA_LD_POST)$(MP_LINKER_FILE_OPTION),--defsym=_min_heap_size=512,--gc-sections,-Map="${DISTDIR}/${PROJECTNAME}.${IMAGE_TYPE}.map",--memorysummary,${DISTDIR}/memoryfile.xml -mdfp="${DFP_DIR}/PIC32CM1216MC00048"
	${MP_CC_DIR}\\xc32-bin2hex ${DISTDIR}/pic32cm_mc00_curiosity_pro.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX} 
endif


# Subprojects
.build-subprojects:


# Subprojects
.clean-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${OBJECTDIR}
	${RM} -r ${DISTDIR}
