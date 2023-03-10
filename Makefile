# Main Makefile for BACnet-stack project with GCC

# tools - only if you need them.
# Most platforms have this already defined
# CC = gcc
# AR = ar
# MAKE = make
# SIZE = size
#
# Assumes rm and cp are available

# configuration
# If BACNET_DEFINES has not already been set, configure to your needs here
MY_BACNET_DEFINES = -DPRINT_ENABLED=1
MY_BACNET_DEFINES += -DBACAPP_ALL
MY_BACNET_DEFINES += -DBACFILE
MY_BACNET_DEFINES += -DINTRINSIC_REPORTING
MY_BACNET_DEFINES += -DBACNET_PROPERTY_LISTS=1
BACNET_DEFINES ?= $(MY_BACNET_DEFINES)

#BACDL_DEFINE=-DBACDL_ETHERNET=1
#BACDL_DEFINE=-DBACDL_ARCNET=1
#BACDL_DEFINE=-DBACDL_MSTP=1
BACDL_DEFINE?=-DBACDL_BIP=1

# Declare your level of BBMD support
BBMD_DEFINE ?=-DBBMD_ENABLED=1
#BBMD_DEFINE ?= -DBBMD_ENABLED=0
#BBMD_DEFINE ?= -DBBMD_CLIENT_ENABLED

# Passing parameters via command line
MAKE_DEFINE ?=

# Define WEAK_FUNC for [...somebody help here; I can't find any uses of it]
DEFINES = $(BACNET_DEFINES) $(BACDL_DEFINE) $(BBMD_DEFINE) -DWEAK_FUNC=
DEFINES += $(MAKE_DEFINE)

# BACnet Ports Directory
BACNET_PORT ?= linux

# Default compiler settings
OPTIMIZATION = -Os
DEBUGGING =
WARNINGS = -Wall -Wmissing-prototypes
ifeq (${BUILD},debug)
OPTIMIZATION = -O0
DEBUGGING = -g -DDEBUG_ENABLED=1
ifeq (${BACDL_DEFINE},-DBACDL_BIP=1)
DEFINES += -DBIP_DEBUG
endif
endif
CFLAGS  = $(WARNINGS) $(DEBUGGING) $(OPTIMIZATION) $(STANDARDS) $(INCLUDES) $(DEFINES) $(USER_CFLAGS)

# Export the variables defined here to all subprocesses
# (see http://www.gnu.org/software/automake/manual/make/Special-Targets.html)
.EXPORT_ALL_VARIABLES:

all: library demos
.PHONY : all library demos clean

library:
	$(MAKE) -s -C lib all

demos:
	$(MAKE) -s -C demo all

gateway:
	$(MAKE) -B -s -C demo gateway

mstpcap:
	$(MAKE) -B -s -C demo mstpcap

router:
	$(MAKE) -s -C demo router

router-ipv6:
	$(MAKE) -B -s -C demo router-ipv6

# Add "ports" to the build, if desired
ports:	atmega168 bdk-atxx4-mstp at91sam7s stm32f10x
	@echo "Built the ARM7 and AVR ports"

atmega168: ports/atmega168/Makefile
	$(MAKE) -s -C ports/atmega168 clean all

at91sam7s: ports/at91sam7s/Makefile
	$(MAKE) -s -C ports/at91sam7s clean all

bdk-atxx4-mstp: ports/bdk-atxx4-mstp/Makefile
	$(MAKE) -s -C ports/bdk-atxx4-mstp clean all

stm32f10x: ports/stm32f10x/Makefile
	$(MAKE) -s -C ports/stm32f10x clean all

clean:
	$(MAKE) -s -C lib clean
	$(MAKE) -s -C demo clean
	$(MAKE) -s -C demo/router clean
	$(MAKE) -s -C demo/router-ipv6 clean
	$(MAKE) -s -C demo/gateway clean
