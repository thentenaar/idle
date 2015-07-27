idle
====

This is a CPU idling driver for Solaris 2.5.1 (i386). It forces the CPU
to halt when idle which should keep it from over-using your host's CPU
resources when run in a VM. It works quite well in VirtualBox, but I
haven't tested it with other virtualization solutions, or on genuine
hardware.

This driver may improve your VMs boot time up to 67%, and will actually
improve performance on multi-core systems with Hyper-Threading. I haven't
done any significant benchmarking, but I observed the following boot
times on my VM, from the kernel banner to the ``login`` prompt:

| CPUs | Without driver | With driver |
| ---- | -------------- | ----------- |
| 1    | 88 seconds     | 23 seconds  |
| 2    | 95 seconds     | 33 seconds  |
| 3    | 95 seconds     | 33 seconds  |
| 4    | 95 seconds     | 33 seconds  |

Requirements
------------

In order to use this driver, you must be running Solaris 2.5.1 (i386).
This will likely also work on Solaris 2.6 but I've not tested it.

To build the driver, you'll need gcc, as well as the Sun kernel headers
and linker.

A binary packages of this driver can be found here:

ftp://ftp.hentenaar.com/~tah/solaris/2.5.1/i386/idle-1.0.pkg

Installation
------------

Building and installing the driver is as easy as:
```
$ su
Password:
# /usr/ccs/bin/make install
  CC idle.c
  LD idle
Intalling idle...
Loading idle...
Done
#
```

The driver will be automatically loaded at boot after installation.
