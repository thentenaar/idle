/**
 * idle - Halt CPU(s) when idle
 * Copyright (C) 2015 Tim Hentenaar.
 *
 * This code is licenced under the Simplified BSD License.
 * See the LICENSE file for details.
 */

#include <sys/psm_types.h>
#include <sys/psm.h>
#include <sys/cmn_err.h>
#include <sys/modctl.h>

/**
 * This function pointer is exported by the kernel, and defaults to
 * generic_idle_cpu() which is simply a null subroutine. This is
 * the function the idle thread calls to idle the CPU on uppc
 * systems.
 */
extern void (*idle_cpu)(void);
static void (*generic_idle_cpu)(void) = NULL;

/* Mapping for the APIC registers. */
static caddr_t apic_base = NULL;
static volatile unsigned long *apic_icr = NULL;
static volatile unsigned long *apic_id  = NULL;
static volatile unsigned long *apic_lvt_timer = NULL;
static volatile unsigned long *apic_timer_ic = NULL;
static int map_apic_called = 0;

/* Initial count of the system timer running on the BP. */
static unsigned long bp_timer_ic = 0;

#define APIC_BASE 0xfee00000
#define APIC_SIZE 0x3f0

/* APIC LVT timer modes and fields. */
#define APIC_TIMER_ONESHOT 0
#define APIC_TIMER_PERIODIC 1
#define APIC_LVT_TIMER_MODE ((*apic_lvt_timer >> 17) & 3)
#define APIC_LVT_TIMER_MASK ((*apic_lvt_timer >> 16) & 1)
#define APIC_LVT_TIMER_VECTOR (*apic_lvt_timer & 0xff)
#define APIC_LVT_TIMER_MASK_SHIFT 16

/**
 * This interrupt should effect a wake-up of the CPU with
 * little side-effect. This will be regarded as a spurious
 * interrupt by the code in psplusmp that handles interrupts.
 *
 * Using the vector that poke_cpu() uses (also used for the
 * clock interrupt) will cause deadlocks with the dispatcher,
 * and/or the clock interrupt handler.
 */
#define TIMER_VECTOR 0xa0

/**
 * Map the APIC registers so that we can use them.
 */
static void map_apic(void)
{
	apic_base = psm_map(APIC_BASE, APIC_SIZE, PROT_READ | PROT_WRITE);

	if (apic_base) {
		apic_timer_ic  = (unsigned long *)(apic_base + 0x380);
		apic_lvt_timer = (unsigned long *)(apic_base + 0x320);
		apic_icr       = (unsigned long *)(apic_base + 0x300);
		apic_id        = (unsigned long *)(apic_base + 0x20);
	}

	map_apic_called = 1;
}

/**
 * The psm routines, set_idlecpu() and unset_idlecpu(), are only called
 * for multi-processor machines, not for uppc. Hence this generic
 * idle_cpu() for uni-processor systems that actually lets the CPU
 * idle.
 */
static void my_idle_cpu(void)
{
	asm volatile(
		"pushf\n\t" /* Save EFLAGS. */
		"sti\n\t"   /* Ensure interrupts are enabled. */
		"hlt\n\t"   /* Halt until the next interrupt. */
		"popf\n\t"  /* Restore EFLAGS. */
	);
}

/**
 * Poke a CPU.
 *
 * AFAICT, this is only called by the idle thread, thus
 * it may be pointless since the CPU will already have awoke
 * in that case , but I'll add it anyway for the sake of pairity
 * with set_idlecpu().
 */
static void idle_unset_idlecpu(processorid_t cpun)
{
	if (CPU->cpu_id != cpun)
		poke_cpu(cpun);
}

/**
 * The idle thread will call this function on multi-processor systems
 * when there's no work left in the CPU's dispatch queue.
 *
 * We make use of the APIC on the APs to setup a watchdog timer
 * that ensures that each CPU wakes at some point to check its
 * task queue again. The BP already uses its timer for clock
 * events, and we base our timer upon that timer's interval.
 *
 * Really, the scheduler should just poke the CPU once its dispatch
 * queue has been updated and the watchdog should only be there
 * as a precaution, but it doesn't AFAICT.
 */
static void idle_set_idlecpu(processorid_t cpun)
{
	/* Save EFLAGS and disable interrupts. */
	asm volatile("pushf ; cli");

	/* If we can't map the APIC registers, just return. */
	if (!map_apic_called) map_apic();
	if (!apic_base) goto ret;

halt:
	/* Set the APIC timer to wake us. */
	if (APIC_LVT_TIMER_MODE != APIC_TIMER_PERIODIC) {
		*apic_timer_ic = (bp_timer_ic ? bp_timer_ic : 0x4c42e50);
		*apic_lvt_timer = TIMER_VECTOR;
	} else bp_timer_ic = *apic_timer_ic;

	asm volatile(
		"sti\n\t" /* Enable interrupts. */
		"hlt\n\t" /* Halt until the next interrupt. */
	);

	/* If there's nothing to do, just sleep. */
	if (!CPU->cpu_disp.disp_nrunnable)
		goto halt;

	/* Mask the one-shot timer. */
	if (APIC_LVT_TIMER_MODE == APIC_TIMER_ONESHOT) {
		*apic_timer_ic = 0;
		*apic_lvt_timer = TIMER_VECTOR | (1 << APIC_LVT_TIMER_MASK_SHIFT);
	}

ret:
	/* Restore EFLAGS. */
	asm volatile("popf");
}

static int idle_probe(void)
{
	return PSM_SUCCESS;
}

/**
 * PSM callback functions.
 */
static struct psm_ops ops = {
	idle_probe,
	NULL, /* softinit */
	NULL, /* picinit */
	NULL, /* intr_enter */
	NULL, /* intr_exit */
	NULL, /* setspl */
	NULL, /* addspl */
	NULL, /* delspl */
	NULL, /* disable_intr */
	NULL, /* enable_intr */
	NULL, /* softlvl_to_irq */
	NULL, /* set_softintr */
	idle_set_idlecpu,
	idle_unset_idlecpu,
	NULL, /* clkinit */
	NULL, /* get_clockirq */
	NULL, /* hrtimeinit */
	NULL, /* gethrtime */
	NULL, /* get_next_processorid */
	NULL, /* cpu_start */
	NULL, /* post_cpu_start */
	NULL, /* shutdown */
	NULL, /* get_iplvect */
	NULL, /* send_ipi */
	NULL, /* report_mem */
};

static struct psm_info info = {
	PSM_INFO_VER01,
	PSM_OWN_OVERRIDE,
	&ops,
	"idle",
	"Halt CPU(s) when idle"
};

static void *mod_handle = NULL;

/**
 * DDI module loading glue.
 */
int _init(void)
{
	/* Install the improved uppc idle function. */
	generic_idle_cpu = idle_cpu;
	idle_cpu = my_idle_cpu;
	return psm_mod_init(&mod_handle, &info);
}

int _info(struct modinfo *modinfo)
{
	return psm_mod_info(&mod_handle, &info, modinfo);
}

int _fini(void)
{
	int ret = psm_mod_fini(&mod_handle, &info);
	if (!ret) {
		idle_cpu = generic_idle_cpu;
		if (apic_base) psm_unmap(apic_base, APIC_SIZE, 1);
	}
	return ret;
}
