# Curiosity CircuitPython board (EV78P89A) of the Curiosity PyKit Explorer
DEVICE       := same51j20a
DFP_DIR      := SAME51_DFP/3.9.267
# Cortex-M4F; the hello image keeps to integer code
ARCH         := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
CMSIS_CORE   := core_cm4.h
# The debug UART's DRE interrupt is SERCOM3 line 0
VCOM_IRQ     := 58
VCOM_HANDLER := SERCOM3_0_Handler
