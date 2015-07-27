#
# idle - Halt x86 CPU(s) when idle
# Copyright (C) 2015 Tim Hentenaar.
#
# This code is licenced under the Simplified BSD License.
# See the LICENSE file for details.
#

CC=/usr/local/bin/gcc
LD=/usr/ccs/bin/ld

all: idle

idle: idle.o
	@echo "  LD idle"
	@$(LD) -r -o idle idle.o

idle.o: idle.c
	@echo "  CC idle.c"
	@$(CC) -Os -D_KERNEL -c -o idle.o idle.c

install: all
	@echo "Intalling idle..."
	@cp idle /kernel/mach/idle
	@chmod 0755 /kernel/mach/idle
	@chown root:sys /kernel/mach/idle
	@fgrep "idle" /etc/mach > /dev/null || echo "idle" >> /etc/mach
	@echo "Loading idle... "
	@modload -p mach/idle
	@echo "Done"

.PHONY: all install
