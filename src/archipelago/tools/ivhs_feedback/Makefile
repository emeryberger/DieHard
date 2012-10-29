obj-m := ivhs_feedback.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
PATCH := $(shell awk '/get_full_page_state/ {print $$1}' /proc/kallsyms)

all:
	@sed -i.orig "s/FFFFFF/$(PATCH)/g" *.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install: 
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	@depmod -a

clean: 
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	if [ -f ivhs_feedback.c.orig ] ; then mv ivhs_feedback.c.orig ivhs_feedback.c ; fi
