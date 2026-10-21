/*
 * A logic analyzer for probes and checked apps: the edges of some pins of
 * one PORT group, recorded by a loop running from RAM (a few cycles a
 * sample) and timed by SysTick over its full 24 bits (349 ms at 48 MHz).
 * A pin muxed to a peripheral still reads its pad when PINCFG.INEN is set,
 * so a peripheral's outputs can be watched without wiring.
 *
 * scope_start() takes SysTick over; systick_start() gives it back to
 * check.h's millisecond clock. Times are CPU cycles; SCOPE_US() converts.
 *
 * Stimulus without wiring: a pin whose function is an input (EIC, a TC
 * capture pin) with PINCFG.PULLEN keeps its pull resistor, and PORT.OUT
 * turns it up or down; on a bare header pin that makes clean edges
 * (scope_pull()).
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#ifndef SCOPE_H
#define SCOPE_H

#include "check.h"

#define SCOPE_RAM       __attribute__((section(".ramfunc"), noinline, \
                                       long_call, unused))
#define SCOPE_US(cyc)   ((cyc) / (CPU_CLOCK_HZ / 1000000U))
#define SCOPE_CYC(us)   ((us) * (CPU_CLOCK_HZ / 1000000U))

typedef struct {
    uint32_t t;         /* cycles since the reference time */
    uint32_t v;         /* IN & mask from then on */
} ScopeEdge;

static inline void scope_start(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = 0x00FFFFFFU;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

/* A reference time for scope_since() and scope_record() */
static inline uint32_t scope_now(void)
{
    return SysTick->VAL;
}

static inline uint32_t scope_since(uint32_t t0)
{
    return (t0 - SysTick->VAL) & 0x00FFFFFFU;
}

/* Busy-wait until @cyc cycles after @t0 */
static inline void scope_wait_until(uint32_t t0, uint32_t cyc)
{
    while (scope_since(t0) < cyc) {
    }
}

/*
 * Record the edges of @mask in *@in until @cyc cycles after @t0 or until
 * @max entries; e[0] is the level when recording starts. Returns entries.
 */
SCOPE_RAM static unsigned int scope_record(volatile const uint32_t *in,
                                           uint32_t mask, uint32_t t0,
                                           uint32_t cyc, ScopeEdge *e,
                                           unsigned int max)
{
    uint32_t last = *in & mask, v, t;
    unsigned int n = 0;

    e[n].t = (t0 - SysTick->VAL) & 0x00FFFFFFU;
    e[n++].v = last;
    for (;;) {
        v = *in & mask;
        t = (t0 - SysTick->VAL) & 0x00FFFFFFU;
        if (v != last) {
            e[n].t = t;
            e[n].v = v;
            last = v;
            if (++n == max) {
                break;
            }
        }
        if (t > cyc) {
            break;
        }
    }
    return n;
}

/* Turn pin @pin of group @g's pull resistor up (1) or down (0) */
static inline void scope_pull(unsigned int g, unsigned int pin, int up)
{
    if (up) {
        PORT_REGS->GROUP[g].PORT_OUTSET = 1U << pin;
    } else {
        PORT_REGS->GROUP[g].PORT_OUTCLR = 1U << pin;
    }
}

/* Route pin @pin of group @g to peripheral function @func, input on */
static inline void scope_pmux(unsigned int g, unsigned int pin,
                              unsigned int func, uint8_t extra)
{
    volatile uint8_t *pmux = &PORT_REGS->GROUP[g].PORT_PMUX[pin / 2U];

    *pmux = (pin & 1U) ? (uint8_t)((*pmux & 0x0FU) | (func << 4)) :
                         (uint8_t)((*pmux & 0xF0U) | func);
    PORT_REGS->GROUP[g].PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk |
                                           PORT_PINCFG_INEN_Msk | extra;
}

/* Back to a plain input (pull off) */
static inline void scope_release(unsigned int g, unsigned int pin)
{
    PORT_REGS->GROUP[g].PORT_PINCFG[pin] = 0;
}

/*
 * " name=t0/t1/..." : the first @k times (us, from the reference) at which
 * bit @bit changes, after entry 0; "name=-" when it never does. The level
 * at the start is " name0=<0|1>".
 */
static inline void scope_put(const char *name, const ScopeEdge *e,
                             unsigned int n, unsigned int bit,
                             unsigned int k)
{
    uint32_t m = 1U << bit;
    unsigned int shown = 0;

    console_putc(' ');
    console_puts(name);
    console_puts("0=");
    console_putc((e[0].v & m) ? '1' : '0');
    console_putc(' ');
    console_puts(name);
    console_putc('=');
    for (unsigned int i = 1; i < n && shown < k; i++) {
        if ((e[i].v ^ e[i - 1].v) & m) {
            if (shown++) {
                console_putc('/');
            }
            console_put_dec(SCOPE_US(e[i].t));
        }
    }
    if (!shown) {
        console_putc('-');
    }
}

#endif /* SCOPE_H */
