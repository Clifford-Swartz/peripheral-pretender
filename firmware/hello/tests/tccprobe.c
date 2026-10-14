/*
 * TCC probe: raw observations of TCC0 in each waveform mode, slow enough
 * (GCLK1 = OSC48M / 250, prescaler /1024: about 5.3 ms per tick) to see
 * every count. Each line is one change of COUNT, INTFLAG, the CMPx and
 * buffer-valid bits of STATUS, or the counting direction (CTRLBSET.DIR).
 * Prints values, checks nothing; used to decide what the twin models.
 *
 * Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
 */

#include "console.h"
#include "sys_init.h"
#include "util.h"

#define T               TCC0_REGS
#define TICK_MS         6U
#ifndef PROBE_ROUNDS
#define PROBE_ROUNDS    31U     /* all; a subset: -DPROBE_ROUNDS=n */
#endif

static void sync(void)
{
    while (T->TCC_SYNCBUSY != 0U) {
    }
}

static void command(uint8_t cmd)
{
    T->TCC_CTRLBSET = cmd;
    sync();
    while ((T->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) != 0U) {
    }
}

static uint32_t count(void)
{
    command(TCC_CTRLBSET_CMD_READSYNC);
    return T->TCC_COUNT;
}

static void hex(uint32_t v, unsigned int digits)
{
    static const char x[] = "0123456789ABCDEF";
    char buf[9];

    for (unsigned int i = 0; i < digits; i++) {
        buf[i] = x[(v >> (4U * (digits - 1U - i))) & 0xFU];
    }
    buf[digits] = 0;
    console_puts(buf);
}

static void setup(uint32_t wave, uint8_t ctrlb, uint32_t per,
                  const uint32_t cc[4])
{
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync();
    T->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV1024;
    T->TCC_WAVE = wave;
    T->TCC_PER = per;
    for (unsigned int i = 0; i < 4U; i++) {
        T->TCC_CC[i] = cc[i];
    }
    T->TCC_EVCTRL = TCC_EVCTRL_CNTSEL_BOUNDARY;
    sync();
    if (ctrlb) {
        T->TCC_CTRLBSET = ctrlb;
        sync();
    }
    T->TCC_INTFLAG = 0xFFFFFFFFU;
}

/*
 * Watch for @ticks ticks: print "count:intflag/cmp+bufv/dir" on change,
 * clearing the flags it saw
 */
static void watch(unsigned int ticks)
{
    uint32_t last = 0xFFFFFFFFU;
    uint32_t ms = ticks * TICK_MS;

    while (ms > 0U) {
        uint32_t c = count();
        uint32_t f = T->TCC_INTFLAG;
        uint32_t s = T->TCC_STATUS;
        uint32_t d = T->TCC_CTRLBSET & TCC_CTRLBSET_DIR_Msk;
        uint32_t key = c << 20 ^ f << 8 ^ (s >> 16) ^ d << 31;

        if (key != last) {
            console_puts(" ");
            hex(c, 2);
            console_puts(":");
            hex(f & 0xF000FU, 5);
            console_puts("/");
            hex(s >> 15, 3);
            console_puts(d ? "d" : "u");
            last = key;
        }
        T->TCC_INTFLAG = f;
        if (systick_ms_elapsed()) {
            ms--;
        }
    }
}

static void run(const char *name, uint32_t wave, uint8_t ctrlb,
                unsigned int ticks)
{
    static const uint32_t cc[4] = { 3, 0, 9, 10 };

    setup(wave, ctrlb, 9, cc);
    console_puts(name);
    T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    sync();
    watch(ticks);
    T->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    sync();
    console_puts("\r\n");
}

int main(void)
{
    uint32_t a, b;

    sys_initialize();
    systick_start();
    console_puts("PP tccprobe: " BOARD_BANNER "\r\n");
    MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC0_Msk;
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_SRC_OSC48M |
                                 GCLK_GENCTRL_DIV(250U) |
                                 GCLK_GENCTRL_GENEN_Msk;
    while (GCLK_REGS->GCLK_SYNCBUSY != 0U) {
    }
    GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] =
        GCLK_PCHCTRL_GEN_GCLK1 | GCLK_PCHCTRL_CHEN_Msk;
    while (!(GCLK_REGS->GCLK_PCHCTRL[TCC0_GCLK_ID] & GCLK_PCHCTRL_CHEN_Msk)) {
    }

    /* Reset values and COUNT without READSYNC */
    T->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    sync();
    console_puts("reset:");
    put_field("STATUS", T->TCC_STATUS, 8);
    put_field("PER", T->TCC_PER, 8);
    put_field("WAVE", T->TCC_WAVE, 8);
    put_field("CTRLBSET", T->TCC_CTRLBSET, 2);
    T->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV1024 | TCC_CTRLA_ENABLE_Msk;
    sync();
    delay_ms(30);
    a = T->TCC_COUNT;
    b = count();
    put_field("COUNT-raw", a, 8);
    put_field("COUNT-sync", b, 8);
    T->TCC_CTRLA = 0;
    sync();
    console_puts("\r\n");

    /* Rounds to run (bit 0: modes, 1: CNTSEL/STOP/buffers, 2: writes) */
    if (!(PROBE_ROUNDS & 3U)) {
        goto round3;
    }
    console_puts("legend: count:intflag(MC3-0,ERR,CNT,TRG,OVF)/"
                 "status>>15(CMP3-0,CCBUFV3-0,PERBUFV..)dir\r\n");
    run("nfrq:", TCC_WAVE_WAVEGEN_NFRQ, 0, 22);
    run("mfrq:", TCC_WAVE_WAVEGEN_MFRQ, 0, 10);
    run("npwm:", TCC_WAVE_WAVEGEN_NPWM, 0, 22);
    run("npwm-down:", TCC_WAVE_WAVEGEN_NPWM, TCC_CTRLBSET_DIR_Msk, 22);
    run("npwm-pol:", TCC_WAVE_WAVEGEN_NPWM | TCC_WAVE_POL0_Msk |
        TCC_WAVE_POL2_Msk, 0, 12);
    run("dsbottom:", TCC_WAVE_WAVEGEN_DSBOTTOM, 0, 38);
    run("dsboth:", TCC_WAVE_WAVEGEN_DSBOTH, 0, 20);
    run("dstop:", TCC_WAVE_WAVEGEN_DSTOP, 0, 20);
    run("dscritical:", TCC_WAVE_WAVEGEN_DSCRITICAL, 0, 20);
    run("oneshot:", TCC_WAVE_WAVEGEN_NPWM, TCC_CTRLBSET_ONESHOT_Msk, 14);

    /* Buffers: PERBUF/CCBUF written mid-period, dual-slope */
    {
        static const uint32_t cc[4] = { 3, 0, 9, 10 };

        setup(TCC_WAVE_WAVEGEN_DSBOTTOM, 0, 9, cc);
        console_puts("buf-dsbottom:");
        T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        sync();
        delay_ms(4U * TICK_MS);
        T->TCC_PERBUF = 5;
        T->TCC_CCBUF[0] = 1;
        put_field("PER", T->TCC_PER, 2);
        put_field("CC0", T->TCC_CC[0], 2);
        watch(30);
        put_field("PER", T->TCC_PER, 2);
        put_field("CC0", T->TCC_CC[0], 2);
        T->TCC_CTRLA = 0;
        sync();
        console_puts("\r\n");
    }

    /* Round 2: CNTSEL, STOP/RETRIGGER, buffers mid-period */
    {
        static const uint32_t cc[4] = { 3, 0, 9, 10 };
        static const char *const sel[] = { "cnt-start:", "cnt-end:",
                                           "cnt-between:", "cnt-boundary:" };

        for (unsigned int i = 0; i < 4U; i++) {
            setup(TCC_WAVE_WAVEGEN_NPWM, 0, 9, cc);
            T->TCC_EVCTRL = TCC_EVCTRL_CNTSEL(i);
            console_puts(sel[i]);
            T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
            sync();
            watch(24);
            console_puts(" |stop");
            command(TCC_CTRLBSET_CMD_STOP);
            watch(3);
            console_puts(" |retrig");
            command(TCC_CTRLBSET_CMD_RETRIGGER);
            watch(4);
            T->TCC_CTRLA = 0;
            sync();
            console_puts("\r\n");
        }

        setup(TCC_WAVE_WAVEGEN_NPWM, 0, 9, cc);
        console_puts("stop:");
        T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        sync();
        delay_ms(5U * TICK_MS + TICK_MS / 2U);
        put_field("before", count(), 2);
        command(TCC_CTRLBSET_CMD_STOP);
        put_field("after", count(), 2);
        put_field("STATUS", T->TCC_STATUS, 8);
        watch(3);
        console_puts(" |retrig");
        command(TCC_CTRLBSET_CMD_RETRIGGER);
        watch(4);
        console_puts(" |retrig-running");
        command(TCC_CTRLBSET_CMD_RETRIGGER);
        watch(3);
        T->TCC_CTRLA = 0;
        sync();
        console_puts("\r\n");

        setup(TCC_WAVE_WAVEGEN_NPWM, 0, 9, cc);
        console_puts("ccbuf-npwm:");
        T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        sync();
        delay_ms(2U * TICK_MS + TICK_MS / 2U);
        T->TCC_CCBUF[0] = 6;
        watch(14);
        T->TCC_CTRLA = 0;
        sync();
        console_puts("\r\n");

        setup(TCC_WAVE_WAVEGEN_NPWM, 0, 9, cc);
        console_puts("perbuf-npwm:");
        T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        sync();
        delay_ms(2U * TICK_MS + TICK_MS / 2U);
        T->TCC_PERBUF = 5;
        watch(14);
        T->TCC_CTRLA = 0;
        sync();
        console_puts("\r\n");

        setup(TCC_WAVE_WAVEGEN_DSBOTTOM, 0, 9, cc);
        console_puts("ccbuf-ds:");
        T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        sync();
        delay_ms(2U * TICK_MS + TICK_MS / 2U);
        T->TCC_CCBUF[0] = 6;
        watch(24);
        T->TCC_CTRLA = 0;
        sync();
        console_puts("\r\n");
    }

round3:
    if (PROBE_ROUNDS & 16U) {
        /*
         * Round 5: the waveform pin. TCC1 WO[2] on PA24 (function F, LED0)
         * read back through the input buffer, next to COUNT and STATUS.CMP0
         */
        tcc_registers_t *t1 = TCC1_REGS;
        uint32_t last = 0xFFFFFFFFU;

        MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_TCC1_Msk;
        t1->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
        while (t1->TCC_SYNCBUSY != 0U) {
        }
        t1->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV1024;
        t1->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
        t1->TCC_PER = 9;
        t1->TCC_CC[0] = 3;
        while (t1->TCC_SYNCBUSY != 0U) {
        }
        PORT_REGS->GROUP[0].PORT_PINCFG[24] = PORT_PINCFG_PMUXEN_Msk |
                                              PORT_PINCFG_INEN_Msk;
        PORT_REGS->GROUP[0].PORT_PMUX[12] =
            (PORT_REGS->GROUP[0].PORT_PMUX[12] & 0xF0U) | 0x5U;
        delay_ms(1);
        console_puts("wo-pin:");
        put_field("disabled", (PORT_REGS->GROUP[0].PORT_IN >> 24) & 1U, 1);
        t1->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        while (t1->TCC_SYNCBUSY != 0U) {
        }
        for (uint32_t ms = 24U * TICK_MS; ms > 0U;) {
            uint32_t c, st, pin, key;

            t1->TCC_CTRLBSET = TCC_CTRLBSET_CMD_READSYNC;
            while (t1->TCC_SYNCBUSY != 0U) {
            }
            while ((t1->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) != 0U) {
            }
            c = t1->TCC_COUNT;
            st = (t1->TCC_STATUS >> TCC_STATUS_CMP0_Pos) & 1U;
            pin = (PORT_REGS->GROUP[0].PORT_IN >> 24) & 1U;
            key = c << 2 | st << 1 | pin;
            if (key != last) {
                console_puts(" ");
                hex(c, 2);
                console_puts(st ? "s" : "-");
                console_puts(pin ? "P" : "_");
                last = key;
            }
            if (systick_ms_elapsed()) {
                ms--;
            }
        }
        t1->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
        delay_ms(1);
        put_field("stopped", (PORT_REGS->GROUP[0].PORT_IN >> 24) & 1U, 1);
        t1->TCC_CTRLA = 0;
        while (t1->TCC_SYNCBUSY != 0U) {
        }
        delay_ms(1);
        put_field("disabled-after", (PORT_REGS->GROUP[0].PORT_IN >> 24) & 1U,
                  1);
        PORT_REGS->GROUP[0].PORT_PINCFG[24] = 0;
        console_puts("\r\n");

        /*
         * PA08 (also TCC1 WO[2], function F, no board load) with the pull
         * resistor: down then up, to tell a driven pad from an undriven
         * one; and the duty cycle from 100000 samples of the pin
         */
        for (unsigned int up = 0; up < 2U; up++) {
            static const uint8_t cases[][2] = { { 3, 1 }, { 3, 2 }, { 9, 3 },
                                                { 3, 0 }, { 3, 4 } };

            PORT_REGS->GROUP[0].PORT_DIRCLR = 1U << 8;
            if (up) {
                PORT_REGS->GROUP[0].PORT_OUTSET = 1U << 8;
            } else {
                PORT_REGS->GROUP[0].PORT_OUTCLR = 1U << 8;
            }
            PORT_REGS->GROUP[0].PORT_PINCFG[8] = PORT_PINCFG_PMUXEN_Msk |
                                                 PORT_PINCFG_INEN_Msk |
                                                 PORT_PINCFG_PULLEN_Msk;
            PORT_REGS->GROUP[0].PORT_PMUX[4] =
                (PORT_REGS->GROUP[0].PORT_PMUX[4] & 0xF0U) | 0x5U;
            t1->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
            while (t1->TCC_SYNCBUSY != 0U) {
            }
            delay_ms(1);
            console_puts(up ? "pa08-pullup:" : "pa08-pulldown:");
            put_field("reset", (PORT_REGS->GROUP[0].PORT_IN >> 8) & 1U, 1);
            for (unsigned int i = 0; i < 5U; i++) {
                uint32_t high = 0;

                t1->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
                while (t1->TCC_SYNCBUSY != 0U) {
                }
                t1->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM;
                t1->TCC_PER = cases[i][0];
                t1->TCC_CC[0] = cases[i][1];
                t1->TCC_CTRLA = TCC_CTRLA_ENABLE_Msk;   /* GCLK1, /1 */
                while (t1->TCC_SYNCBUSY != 0U) {
                }
                delay_ms(1);
                for (uint32_t n = 0; n < 100000U; n++) {
                    high += (PORT_REGS->GROUP[0].PORT_IN >> 8) & 1U;
                }
                console_puts(" per");
                hex(cases[i][0], 1);
                console_puts("cc");
                hex(cases[i][1], 1);
                put_field("", high / 1000U, 2);     /* percent */
            }
            t1->TCC_CTRLBSET = TCC_CTRLBSET_CMD_STOP;
            delay_ms(1);
            put_field("stopped", (PORT_REGS->GROUP[0].PORT_IN >> 8) & 1U, 1);
            t1->TCC_CTRLA = 0;
            while (t1->TCC_SYNCBUSY != 0U) {
            }
            delay_ms(1);
            put_field("disabled", (PORT_REGS->GROUP[0].PORT_IN >> 8) & 1U, 1);
            PORT_REGS->GROUP[0].PORT_PINCFG[8] = 0;
            console_puts("\r\n");
        }
    }
    if (PROBE_ROUNDS & 8U) {
        /* Round 4: does an event action keep ENABLE from starting? */
        static const struct {
            const char *name;
            uint32_t ev;
            uint8_t ctrlb;
        } r4[] = {
            { "ev-off:", 0, 0 },
            { "ev0-inc:", TCC_EVCTRL_EVACT0_INC | TCC_EVCTRL_TCEI0_Msk, 0 },
            { "ev0-inc-noei:", TCC_EVCTRL_EVACT0_INC, 0 },
            { "ev0-countev:", TCC_EVCTRL_EVACT0_COUNTEV |
              TCC_EVCTRL_TCEI0_Msk, 0 },
            { "ev1-dec:", TCC_EVCTRL_EVACT1_DEC | TCC_EVCTRL_TCEI1_Msk, 0 },
            { "ev1-stop:", TCC_EVCTRL_EVACT1_STOP | TCC_EVCTRL_TCEI1_Msk, 0 },
            { "ev1-dir:", TCC_EVCTRL_EVACT1_DIR | TCC_EVCTRL_TCEI1_Msk, 0 },
            { "ev0-inc-down:", TCC_EVCTRL_EVACT0_INC | TCC_EVCTRL_TCEI0_Msk,
              TCC_CTRLBSET_DIR_Msk },
        };
        static const uint32_t cc[4] = { 3, 0, 9, 10 };

        MCLK_REGS->MCLK_APBCMASK |= MCLK_APBCMASK_EVSYS_Msk;
        GCLK_REGS->GCLK_PCHCTRL[EVSYS_GCLK_ID_0] =
            GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
        GCLK_REGS->GCLK_PCHCTRL[EVSYS_GCLK_ID_1] =
            GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;
        for (unsigned int i = 0; i < sizeof(r4) / sizeof(r4[0]); i++) {
            uint32_t c0, c1, c2, c3, st0;

            EVSYS_REGS->EVSYS_CTRLA = EVSYS_CTRLA_SWRST_Msk;
            while (EVSYS_REGS->EVSYS_CTRLA & EVSYS_CTRLA_SWRST_Msk) {
            }
            EVSYS_REGS->EVSYS_CHANNEL[0] = EVSYS_CHANNEL_PATH_SYNCHRONOUS |
                                           EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
            EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_PATH_SYNCHRONOUS |
                                           EVSYS_CHANNEL_EDGSEL_RISING_EDGE;
            EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TCC0_EV_0] = 1;
            EVSYS_REGS->EVSYS_USER[EVENT_ID_USER_TCC0_EV_1] = 2;
            setup(TCC_WAVE_WAVEGEN_NPWM, r4[i].ctrlb, 9, cc);
            T->TCC_EVCTRL = r4[i].ev;
            sync();
            T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
            sync();
            delay_ms(3U * TICK_MS + TICK_MS / 2U);
            st0 = T->TCC_STATUS;
            c0 = count();
            EVSYS_REGS->EVSYS_SWEVT = 1;        /* EV0 */
            delay_us(100);
            c1 = count();
            EVSYS_REGS->EVSYS_SWEVT = 2;        /* EV1 */
            delay_us(100);
            c2 = count();
            delay_ms(2U * TICK_MS);
            c3 = count();
            console_puts(r4[i].name);
            put_field("stop", st0 & 1U, 1);
            put_field("c0", c0, 2);
            put_field("ev0", c1, 2);
            put_field("ev1", c2, 2);
            put_field("2t", c3, 2);
            put_field("st", T->TCC_STATUS & 1U, 1);
            T->TCC_CTRLA = 0;
            sync();
            console_puts("\r\n");
        }
    }
    if (!(PROBE_ROUNDS & 4U)) {
        test_exit();
    }
    /* Round 3: which writes disturb a running counter (PER 99, CC0 10) */
    for (unsigned int w = 0; w < 9U; w++) {
        static const char *const what[] = {
            "w-ccbuf:", "w-cc:", "w-perbuf:", "w-per:", "w-lupd-ccbuf:",
            "w-wave:", "w-count:", "w-dbgctrl:", "w-none:" };
        static const uint32_t cc[4] = { 10, 0, 90, 200 };
        uint32_t c0, c1, c2, f1, f2, st, sb0, sb1, sb2;

        setup(TCC_WAVE_WAVEGEN_NPWM, w == 4U ? TCC_CTRLBSET_LUPD_Msk : 0, 99,
              cc);
        T->TCC_EVCTRL = 0;
        sync();
        T->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
        sync();
        delay_ms(20U * TICK_MS);
        T->TCC_INTFLAG = 0xFFFFFFFFU;
        c0 = count();
        switch (w) {
        case 0: case 4: T->TCC_CCBUF[0] = 50; break;
        case 1: T->TCC_CC[0] = 50; break;
        case 2: T->TCC_PERBUF = 80; break;
        case 3: T->TCC_PER = 80; break;
        case 5: T->TCC_WAVE = TCC_WAVE_WAVEGEN_NPWM; break;
        case 6: T->TCC_COUNT = 30; break;
        case 7: T->TCC_DBGCTRL = 0; break;
        default: break;
        }
        sb0 = T->TCC_SYNCBUSY;
        for (uint32_t n = 0; n < 2000U && T->TCC_SYNCBUSY != 0U; n++) {
            delay_us(10);
        }
        sb1 = T->TCC_SYNCBUSY;
        if (sb1 != 0U) {
            T->TCC_CTRLBSET = TCC_CTRLBSET_CMD_UPDATE;  /* unblock */
            delay_us(500);
        }
        sb2 = T->TCC_SYNCBUSY;
        c1 = count();
        f1 = T->TCC_INTFLAG;
        delay_ms(2U * TICK_MS);
        c2 = count();
        f2 = T->TCC_INTFLAG;
        st = T->TCC_STATUS;
        console_puts(what[w]);
        put_field("sb0", sb0, 4);
        put_field("sb20ms", sb1, 4);
        put_field("sb-upd", sb2, 4);
        put_field("c0", c0, 2);
        put_field("c1", c1, 2);
        put_field("c2", c2, 2);
        put_field("f1", f1, 5);
        put_field("f2", f2, 5);
        put_field("st", st, 8);
        put_field("CC0", T->TCC_CC[0], 2);
        put_field("PER", T->TCC_PER, 2);
        T->TCC_CTRLA = 0;
        sync();
        console_puts("\r\n");
    }
    test_exit();
}
