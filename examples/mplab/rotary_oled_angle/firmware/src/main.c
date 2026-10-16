/*******************************************************************************
  Rotary angle on an OLED: PIC32CM MC00 Curiosity Pro (EV15N46A)

  File Name:
    main.c

  Summary:
    A Rotary G Click on EXT1 sets an angle; an OLED B Click on EXT2 shows it,
    as big digits and as a dial; the Rotary's LED ring points the same way.

  Description:
    Built on Microchip's Harmony CSP example port_led_on_off_polling for
    this board (its generated clock, port and start-up code are unchanged);
    everything here talks to the registers directly.

    Wiring (two mikroBUS Xplained Pro adapters):
      EXT1, Rotary G Click: encoder A on PWM (PB10), B on AN (PB08), push
        switch on INT (PB02, high while pressed; the PORT's pull-down),
        16-LED ring on two 74HC595 on SERCOM1 SPI (MOSI PA18, SCK PA19,
        MISO PA16), LATCH on CS (PA17), MR# on RST (PA20).
      EXT2, OLED B Click: SSD1306, 96x39 blue OLED, on SERCOM2 I2C (SDA
        PA12, SCL PA13): commands to 0x3C with D/C (PWM, PB22) low, data to
        0x3D with D/C high; RES# on RST (PA10).
      VCOM: SERCOM0 (PA06 TX, PA07 RX), 115200 8N1: the angle as it moves.

    The encoder has 30 detents a turn: 12 degrees each, clockwise up,
    counted in the EIC interrupt on both edges of A and B. The
    angle counts from 0 at twelve o'clock; pushing the knob sets it back
    to 0; LED0 lights while the knob is pushed.

    Runs unchanged on the board and on its Peripheral Pretender twin
    (-M pic32cm-mc00-cpro,ext1=rotaryg,ext2=oledb, or the web board view /
    VS Code extension: plug Rotary G into EXT1 and OLED B into EXT2).
 *******************************************************************************/

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdint.h>
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <string.h>
#include "definitions.h"                // SYS function prototypes

#define PA                  (&PORT_REGS->GROUP[0])
#define PB                  (&PORT_REGS->GROUP[1])

/* EXT1: Rotary G Click */
#define ENC_A_PIN           10U     /* PB10 */
#define ENC_B_PIN           8U      /* PB08 */
#define ENC_SW_PIN          2U      /* PB02 */
#define RING_LATCH_PIN      17U     /* PA17 */
#define RING_MR_PIN         20U     /* PA20 */
#define DEGREES_PER_DETENT  12      /* 30 detents a turn */

/* EXT2: OLED B Click */
#define OLED_DC_PIN         22U     /* PB22: SA0 on I2C */
#define OLED_RST_PIN        10U     /* PA10 */
#define OLED_CMD_ADDR       0x3CU
#define OLED_DATA_ADDR      0x3DU
#define OLED_W              96U
#define OLED_H              39U
#define OLED_PAGES          5U

#define SPI                 (&SERCOM1_REGS->SPIM)
#define I2C                 (&SERCOM2_REGS->I2CM)
#define UART                (&SERCOM0_REGS->USART_INT)

static uint8_t frame[OLED_PAGES][OLED_W];

/* sin(6 degrees x i) x 1000, i = 0..59 */
static const int16_t sin6[60] = {
    0, 105, 208, 309, 407, 500, 588, 669, 743, 809, 866, 914, 951, 978,
    995, 1000, 995, 978, 951, 914, 866, 809, 743, 669, 588, 500, 407, 309,
    208, 105, 0, -105, -208, -309, -407, -500, -588, -669, -743, -809,
    -866, -914, -951, -978, -995, -1000, -995, -978, -951, -914, -866,
    -809, -743, -669, -588, -500, -407, -309, -208, -105,
};

/* 5x7 font, a column a byte, LSB at the top */
static const uint8_t font_digits[10][5] = {
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, { 0x00, 0x42, 0x7F, 0x40, 0x00 },
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, { 0x21, 0x41, 0x45, 0x4B, 0x31 },
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, { 0x27, 0x45, 0x45, 0x45, 0x39 },
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, { 0x01, 0x71, 0x09, 0x05, 0x03 },
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, { 0x06, 0x49, 0x49, 0x29, 0x1E },
};
static const uint8_t font_angle[5][5] = {               /* A N G L E */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, { 0x7F, 0x04, 0x08, 0x10, 0x7F },
    { 0x3E, 0x41, 0x49, 0x49, 0x7A }, { 0x7F, 0x40, 0x40, 0x40, 0x40 },
    { 0x7F, 0x49, 0x49, 0x49, 0x41 },
};
static const uint8_t font_degree[5] = { 0x06, 0x09, 0x09, 0x06, 0x00 };

// *****************************************************************************
// Section: Timing (SysTick, polled: the example installs no handler)
// *****************************************************************************

static void delay_ms(uint32_t ms)
{
    SysTick->LOAD = (CPU_CLOCK_FREQUENCY / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
    while (ms-- > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
        }
    }
    SysTick->CTRL = 0U;
}

// *****************************************************************************
// Section: Peripheral set-up (registers; the example's MCC project has only
// the port and clock)
// *****************************************************************************

static void pin_function(port_group_registers_t *g, unsigned int pin,
                         uint8_t func)
{
    g->PORT_PINCFG[pin] = PORT_PINCFG_PMUXEN_Msk;
    g->PORT_PMUX[pin / 2U] = (g->PORT_PMUX[pin / 2U] &
                              ((pin & 1U) != 0U ? 0x0FU : 0xF0U)) |
                             (uint8_t)(func << ((pin & 1U) * 4U));
}

static void peripheral_clock(uint32_t gclk_id, uint32_t apbc_mask)
{
    MCLK_REGS->MCLK_APBCMASK |= apbc_mask;
    GCLK_REGS->GCLK_PCHCTRL[gclk_id] = GCLK_PCHCTRL_GEN_GCLK0 |
                                       GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[gclk_id] & GCLK_PCHCTRL_CHEN_Msk) == 0U)
    {
    }
}

static void uart_init(void)
{
    peripheral_clock(SERCOM0_GCLK_ID_CORE, MCLK_APBCMASK_SERCOM0_Msk);
    pin_function(PA, 6U, 3U);           /* PAD2: TX, function D */
    pin_function(PA, 7U, 3U);           /* PAD3: RX */
    UART->SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK |
                         SERCOM_USART_INT_CTRLA_RXPO(3U) |
                         SERCOM_USART_INT_CTRLA_TXPO(1U) |
                         SERCOM_USART_INT_CTRLA_DORD_Msk;
    UART->SERCOM_BAUD = 63019U;         /* 115200 at 48 MHz */
    UART->SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT |
                         SERCOM_USART_INT_CTRLB_TXEN_Msk;
    while (UART->SERCOM_SYNCBUSY != 0U)
    {
    }
    UART->SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;
    while (UART->SERCOM_SYNCBUSY != 0U)
    {
    }
}

static void uart_puts(const char *s)
{
    while (*s != '\0')
    {
        while ((UART->SERCOM_INTFLAG & SERCOM_USART_INT_INTFLAG_DRE_Msk) == 0U)
        {
        }
        UART->SERCOM_DATA = (uint8_t)*s++;
    }
}

static void spi_init(void)
{
    peripheral_clock(SERCOM1_GCLK_ID_CORE, MCLK_APBCMASK_SERCOM1_Msk);
    pin_function(PA, 16U, 2U);          /* PAD0: MISO, function C */
    pin_function(PA, 18U, 2U);          /* PAD2: MOSI */
    pin_function(PA, 19U, 2U);          /* PAD3: SCK */
    SPI->SERCOM_CTRLA = SERCOM_SPIM_CTRLA_MODE_SPI_MASTER |
                        SERCOM_SPIM_CTRLA_DOPO(1U) |
                        SERCOM_SPIM_CTRLA_DIPO(0U);
    SPI->SERCOM_CTRLB = SERCOM_SPIM_CTRLB_RXEN_Msk;
    SPI->SERCOM_BAUD = 23U;             /* 1 MHz */
    while (SPI->SERCOM_SYNCBUSY != 0U)
    {
    }
    SPI->SERCOM_CTRLA |= SERCOM_SPIM_CTRLA_ENABLE_Msk;
    while (SPI->SERCOM_SYNCBUSY != 0U)
    {
    }
}

static void spi_byte(uint8_t v)
{
    while ((SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_DRE_Msk) == 0U)
    {
    }
    SPI->SERCOM_DATA = v;
    while ((SPI->SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk) == 0U)
    {
    }
    (void)SPI->SERCOM_DATA;
}

static void i2c_sync(void)
{
    while (I2C->SERCOM_SYNCBUSY != 0U)
    {
    }
}

static void i2c_init(void)
{
    peripheral_clock(SERCOM2_GCLK_ID_CORE, MCLK_APBCMASK_SERCOM2_Msk);
    pin_function(PA, 12U, 2U);          /* PAD0: SDA, function C */
    pin_function(PA, 13U, 2U);          /* PAD1: SCL */
    I2C->SERCOM_CTRLA = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
                        SERCOM_I2CM_CTRLA_SDAHOLD(2U);
    I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk;
    I2C->SERCOM_BAUD = SERCOM_I2CM_BAUD_BAUD(55U);      /* about 400 kHz */
    i2c_sync();
    I2C->SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_ENABLE_Msk;
    i2c_sync();
    I2C->SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(1U);   /* idle */
    i2c_sync();
}

static bool i2c_wait(void)
{
    for (uint32_t n = 0U; n < 100000U; n++)
    {
        if ((I2C->SERCOM_INTFLAG & (SERCOM_I2CM_INTFLAG_MB_Msk |
                                    SERCOM_I2CM_INTFLAG_ERROR_Msk)) != 0U)
        {
            return (I2C->SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk) == 0U;
        }
    }
    return false;
}

static void i2c_stop(void)
{
    I2C->SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk |
                        SERCOM_I2CM_CTRLB_CMD(3U);
    i2c_sync();
}

/* @control (the SSD1306's control byte), then @n bytes */
static void i2c_send(uint8_t addr, uint8_t control, const uint8_t *d,
                     unsigned int n)
{
    I2C->SERCOM_ADDR = (uint32_t)addr << 1;
    i2c_sync();
    if (i2c_wait())
    {
        I2C->SERCOM_DATA = control;
        (void)i2c_wait();
        for (unsigned int i = 0U; i < n; i++)
        {
            I2C->SERCOM_DATA = d[i];
            (void)i2c_wait();
        }
    }
    i2c_stop();
}

// *****************************************************************************
// Section: Rotary G Click
// *****************************************************************************

static void ring_init(void)
{
    PA->PORT_OUTCLR = 1UL << RING_LATCH_PIN;
    PA->PORT_OUTSET = 1UL << RING_MR_PIN;
    PA->PORT_DIRSET = (1UL << RING_LATCH_PIN) | (1UL << RING_MR_PIN);
    PB->PORT_OUTCLR = 1UL << ENC_SW_PIN;                /* pull-down */
    PB->PORT_PINCFG[ENC_SW_PIN] = PORT_PINCFG_INEN_Msk |
                                  PORT_PINCFG_PULLEN_Msk;
}

/* Bit n lights LED n+1 (LED1 at six o'clock, clockwise) */
static void ring_show(uint16_t leds)
{
    PA->PORT_OUTCLR = 1UL << RING_LATCH_PIN;
    spi_byte((uint8_t)(leds >> 8));
    spi_byte((uint8_t)leds);
    PA->PORT_OUTSET = 1UL << RING_LATCH_PIN;             /* STCP rising */
}

static unsigned int enc_pin(unsigned int pin)
{
    return (PB->PORT_IN >> pin) & 1U;
}

// *****************************************************************************
// Section: OLED B Click (SSD1306 on I2C)
// *****************************************************************************

static void oled_commands(const uint8_t *c, unsigned int n)
{
    PB->PORT_OUTCLR = 1UL << OLED_DC_PIN;
    i2c_send(OLED_CMD_ADDR, 0x00U, c, n);
}

static void oled_init(void)
{
    /* MikroElektronika's oledb set-up: MUX 40, charge pump, C8h, DAh 12h */
    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x27, 0xD3, 0x00, 0x40, 0x8D, 0x14,
        0xC8, 0xDA, 0x12, 0x81, 0xAF, 0xD9, 0x25, 0xDB, 0x20, 0xA4,
        0xA6, 0xAF,
    };

    PB->PORT_DIRSET = 1UL << OLED_DC_PIN;
    PA->PORT_DIRSET = 1UL << OLED_RST_PIN;
    PA->PORT_OUTCLR = 1UL << OLED_RST_PIN;               /* reset pulse */
    delay_ms(1U);
    PA->PORT_OUTSET = 1UL << OLED_RST_PIN;
    delay_ms(1U);
    oled_commands(init, sizeof(init));
}

/* With that set-up, screen row y is RAM line y + 1, column x RAM 95 - x */
static void plot(int x, int y)
{
    if (x >= 0 && x < (int)OLED_W && y >= 0 && y < (int)OLED_H)
    {
        unsigned int line = (unsigned int)y + 1U;

        frame[line / 8U][OLED_W - 1U - (unsigned int)x] |=
            (uint8_t)(1U << (line % 8U));
    }
}

/* A 5x7 glyph at (@x, @y), each pixel @s x @s */
static void glyph(const uint8_t *g, int x, int y, int s)
{
    for (int c = 0; c < 5; c++)
    {
        for (int r = 0; r < 7; r++)
        {
            if (((g[c] >> r) & 1U) != 0U)
            {
                for (int dy = 0; dy < s; dy++)
                {
                    for (int dx = 0; dx < s; dx++)
                    {
                        plot(x + (c * s) + dx, y + (r * s) + dy);
                    }
                }
            }
        }
    }
}

static void line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;

    for (;;)
    {
        int e2 = 2 * err;               /* taken once per step */

        plot(x0, y0);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

/* The angle as "ANGLE", big digits and a dial with its needle */
static void draw(int angle)
{
    const int cx = 77, cy = 19, r = 17;
    char digits[4];
    int n = 0, x;

    memset(frame, 0, sizeof(frame));
    for (int i = 0; i < 5; i++)
    {
        glyph(font_angle[i], 2 + (i * 6), 1, 1);
    }
    do
    {
        digits[n++] = (char)('0' + (angle % 10));
        angle /= 10;
    } while (angle > 0);
    x = 2;
    while (n > 0)
    {
        glyph(font_digits[digits[--n] - '0'], x, 13, 3);
        x += 18;
    }
    glyph(font_degree, x - 1, 12, 2);
    for (int i = 0; i < 60; i++)                        /* the dial */
    {
        plot(cx + ((r * sin6[i]) / 1000), cy - ((r * sin6[(i + 15) % 60]) /
                                                1000));
    }
    for (int i = 0; i < 60; i += 15)                    /* N, E, S, W */
    {
        line(cx + (((r - 3) * sin6[i]) / 1000),
             cy - (((r - 3) * sin6[(i + 15) % 60]) / 1000),
             cx + ((r * sin6[i]) / 1000),
             cy - ((r * sin6[(i + 15) % 60]) / 1000));
    }
}

static void oled_flush(void)
{
    for (unsigned int p = 0U; p < OLED_PAGES; p++)
    {
        uint8_t at[3] = { (uint8_t)(0xB0U | p), 0x10U, 0x00U };

        oled_commands(at, sizeof(at));
        PB->PORT_OUTSET = 1UL << OLED_DC_PIN;
        i2c_send(OLED_DATA_ADDR, 0x40U, frame[p], OLED_W);
    }
}

static void show_angle(int angle)
{
    const int cx = 77, cy = 19, len = 13;
    int i6 = angle / 6;                 /* angles are multiples of 12 */
    char text[24] = "angle ";
    int n = 6, a = angle, k;
    char rev[4];

    draw(angle);
    line(cx, cy, cx + ((len * sin6[i6]) / 1000),
         cy - ((len * sin6[(i6 + 15) % 60]) / 1000));
    oled_flush();
    /* LED9 at twelve o'clock; 22.5 degrees a LED, clockwise */
    ring_show((uint16_t)(1U << ((8U + (((unsigned int)angle * 2U + 22U) /
                                       45U)) % 16U)));
    k = 0;
    do
    {
        rev[k++] = (char)('0' + (a % 10));
        a /= 10;
    } while (a > 0);
    while (k > 0)
    {
        text[n++] = rev[--k];
    }
    text[n++] = '\r';
    text[n++] = '\n';
    text[n] = '\0';
    uart_puts(text);
}

// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************

// *****************************************************************************
// Section: Encoder, counted in the EIC interrupt (both edges of A and B), so
// a redraw of the OLED, about 12 ms of I2C, never makes it miss a step
// *****************************************************************************

static volatile int detents;
static unsigned int enc_start, enc_held;

/* MikroElektronika's way: at a detent A = B; the first edge that leaves
   A != B is one step, clockwise when A has moved */
void EIC_Handler(void)
{
    unsigned int a, b;

    EIC_REGS->EIC_INTFLAG = EIC_REGS->EIC_INTFLAG;
    a = enc_pin(ENC_A_PIN);
    b = enc_pin(ENC_B_PIN);
    if (a == b)
    {
        enc_start = a;
        enc_held = 0U;
    }
    else if (enc_held == 0U)
    {
        detents += (a != enc_start) ? 1 : -1;
        enc_held = 1U;
    }
}

static void encoder_init(void)
{
    GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] = GCLK_PCHCTRL_GEN_GCLK0 |
                                          GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[EIC_GCLK_ID] &
            GCLK_PCHCTRL_CHEN_Msk) == 0U)
    {
    }
    /* PB08 = EXTINT8 (B), PB10 = EXTINT10 (A): function A, input on */
    pin_function(PB, ENC_B_PIN, 0U);
    pin_function(PB, ENC_A_PIN, 0U);
    PB->PORT_PINCFG[ENC_B_PIN] |= PORT_PINCFG_INEN_Msk;
    PB->PORT_PINCFG[ENC_A_PIN] |= PORT_PINCFG_INEN_Msk;
    EIC_REGS->EIC_CTRLA = 0U;
    while (EIC_REGS->EIC_SYNCBUSY != 0U)
    {
    }
    /* CONFIG1 holds EXTINT8..15, 4 bits each: SENSE = 3, both edges */
    EIC_REGS->EIC_CONFIG1 = (3UL << ((ENC_B_PIN - 8U) * 4U)) |
                              (3UL << ((ENC_A_PIN - 8U) * 4U));
    EIC_REGS->EIC_INTENSET = (1UL << ENC_B_PIN) | (1UL << ENC_A_PIN);
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U)
    {
    }
    enc_start = enc_pin(ENC_A_PIN) & enc_pin(ENC_B_PIN);
    NVIC_EnableIRQ(EIC_IRQn);
}

// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************

int main ( void )
{
    int shown = -1;
    unsigned int pushed = 0U;

    /* Initialize all modules */
    SYS_Initialize ( NULL );

    uart_init();
    uart_puts("Rotary angle on an OLED: Rotary G on EXT1, OLED B on EXT2\r\n");
    spi_init();
    i2c_init();
    ring_init();
    oled_init();
    encoder_init();

    while ( true )
    {
        unsigned int sw = enc_pin(ENC_SW_PIN);
        int angle;

        if (sw != pushed)
        {
            if (sw != 0U)
            {
                detents = 0;            /* pushed: back to zero */
                LED_Clear();            /* LED0 on while pushed */
            }
            else
            {
                LED_Set();
            }
            pushed = sw;
        }
        angle = ((detents % 30) + 30) % 30 * DEGREES_PER_DETENT;
        if (angle != shown)
        {
            show_angle(angle);
            shown = angle;
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/
