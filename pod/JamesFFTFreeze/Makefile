# Project Name
TARGET = JamesFFTFreeze

# Use DaisySP-LGPL for this project
USE_DAISYSP_LGPL = 1

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP


# Sources
CPP_SOURCES = JamesFFTFreeze.cpp \
    $(DAISYSP_DIR)/DaisySP-LGPL/Source/Effects/reverbsc.cpp

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core

$(info >>>> C_INCLUDES = $(C_INCLUDES))

include $(SYSTEM_FILES_DIR)/Makefile