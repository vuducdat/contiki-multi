CONTIKI = ../../..
APPS = powertrace collect-view

CONTIKI_PROJECT = udp-sender udp-sink
PROJECT_SOURCEFILES += collect-common.c
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

WITH_UIP6=1
UIP_CONF_IPV6=1
CFLAGS+= -DUIP_CONF_IPV6_RPL
SMALL=1

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include
