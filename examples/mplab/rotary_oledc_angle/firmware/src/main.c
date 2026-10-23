/*******************************************************************************
  Rotary angle on a colour OLED: PIC32CM MC00 Curiosity Pro (EV15N46A)

  File Name:
    main.c

  Summary:
    A Rotary R Click on EXT2 sets an angle; an OLED C Click on EXT1 shows it,
    as big digits and as a dial with a needle; the Rotary's LED ring points
    the same way.

  Description:
    Built on Microchip's Harmony CSP example port_led_on_off_polling for
    this board (its generated clock, port and start-up code are unchanged);
    everything here talks to the registers directly.

    Wiring (two mikroBUS Xplained Pro adapters, one SPI bus on SERCOM1:
    MOSI PA18, SCK PA19, MISO PA16):
      EXT1, OLED C Click: SSD1351, 96x96 RGB OLED: CS on CS (PA17), D/C on
        PWM (PB10), RES# on RST (PA20), the 15 V supply's EN on INT (PB02).
      EXT2, Rotary R Click: encoder A on PWM (PB22, EXTINT6), B on AN
        (PA08: its EIC line is the NMI, so it is read as an input), push
        switch on INT (PA28, high while pressed; the PORT's pull-down),
        16-LED ring on two 74HC595, LATCH on CS (PA15), MR# on RST (PA10).
      VCOM: SERCOM0 (PA06 TX, PA07 RX), 115200 8N1: the angle as it moves.

    The encoder has 30 detents a turn: 12 degrees each, clockwise up. Each
    detent move holds one edge of A: counted in the EIC interrupt on both
    edges of A, clockwise when A and B differ after it. The angle counts
    from 0 at twelve o'clock; pushing the knob sets it back to 0; LED0
    lights while the knob is pushed.

    The SSD1351 set-up is MikroElektronika's oledc driver's: columns 16-111
    and rows 0-95 are the panel, colours RGB565, high byte first. With 16 KB
    of RAM there is no frame buffer: the picture is drawn in windows.

    Runs unchanged on the board and on its Peripheral Pretender twin
    (-M pic32cm-mc00-cpro,ext1=oledc,ext2=rotaryr, or the web board view /
    VS Code extension: plug OLED C into EXT1 and Rotary R into EXT2).
 *******************************************************************************/

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdint.h>
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes

#define PA                  (&PORT_REGS->GROUP[0])
#define PB                  (&PORT_REGS->GROUP[1])

/* EXT1: OLED C Click */
#define OLED_CS_PIN         17U     /* PA17 */
#define OLED_DC_PIN         10U     /* PB10 */
#define OLED_RST_PIN        20U     /* PA20 */
#define OLED_EN_PIN         2U      /* PB02 */
#define OLED_COL0           16U     /* the panel's first column */
#define OLED_SIZE           96

/* EXT2: Rotary R Click */
#define ENC_A_PIN           22U     /* PB22, EXTINT6 */
#define ENC_A_EXTINT        6U
#define ENC_B_PIN           8U      /* PA08 */
#define ENC_SW_PIN          28U     /* PA28 */
#define RING_LATCH_PIN      15U     /* PA15 */
#define RING_MR_PIN         10U     /* PA10 */
#define DEGREES_PER_DETENT  12      /* 30 detents a turn */

#define SPI                 (&SERCOM1_REGS->SPIM)
#define UART                (&SERCOM0_REGS->USART_INT)

/* RGB565 */
#define BLACK               0x0000U
#define WHITE               0xFFFFU
#define GREY                0x8410U
#define CYAN                0x07FFU
#define YELLOW              0xFFE0U
#define RED                 0xF800U

/* The dial */
#define DIAL_X              48
#define DIAL_Y              66
#define DIAL_R              26
#define NEEDLE              19

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
    SPI->SERCOM_BAUD = 1U;              /* 12 MHz */
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

// *****************************************************************************
// Section: Rotary R Click (EXT2)
// *****************************************************************************

static void ring_init(void)
{
    PA->PORT_OUTCLR = 1UL << RING_LATCH_PIN;
    PA->PORT_OUTSET = 1UL << RING_MR_PIN;
    PA->PORT_DIRSET = (1UL << RING_LATCH_PIN) | (1UL << RING_MR_PIN);
    PA->PORT_OUTCLR = 1UL << ENC_SW_PIN;                /* pull-down */
    PA->PORT_PINCFG[ENC_SW_PIN] = PORT_PINCFG_INEN_Msk |
                                  PORT_PINCFG_PULLEN_Msk;
}

/* Bit n lights LED n+1 (LED1 at six o'clock, clockwise). The 74HC595s see
   every byte on the bus; only the last 16 before the latch show. */
static void ring_show(uint16_t leds)
{
    PA->PORT_OUTCLR = 1UL << RING_LATCH_PIN;
    spi_byte((uint8_t)(leds >> 8));
    spi_byte((uint8_t)leds);
    PA->PORT_OUTSET = 1UL << RING_LATCH_PIN;             /* STCP rising */
}

// *****************************************************************************
// Section: OLED C Click (SSD1351 on SPI, EXT1)
// *****************************************************************************

/* Command @cmd with its @n parameters (D/C high for the parameters) */
static void oled_command(uint8_t cmd, const uint8_t *p, unsigned int n)
{
    PA->PORT_OUTCLR = 1UL << OLED_CS_PIN;
    PB->PORT_OUTCLR = 1UL << OLED_DC_PIN;
    spi_byte(cmd);
    PB->PORT_OUTSET = 1UL << OLED_DC_PIN;
    for (unsigned int i = 0U; i < n; i++)
    {
        spi_byte(p[i]);
    }
    PA->PORT_OUTSET = 1UL << OLED_CS_PIN;
}

static void oled_command1(uint8_t cmd, uint8_t p)
{
    oled_command(cmd, &p, 1U);
}

static void oled_init(void)
{
    static const uint8_t contrast[3] = { 0x8A, 0x51, 0x8A };
    static const uint8_t vsl[3] = { 0xA0, 0xB5, 0x55 };

    PA->PORT_OUTSET = (1UL << OLED_CS_PIN) | (1UL << OLED_RST_PIN);
    PA->PORT_DIRSET = (1UL << OLED_CS_PIN) | (1UL << OLED_RST_PIN);
    PB->PORT_OUTSET = 1UL << OLED_EN_PIN;               /* 15 V on */
    PB->PORT_DIRSET = (1UL << OLED_DC_PIN) | (1UL << OLED_EN_PIN);
    PA->PORT_OUTCLR = 1UL << OLED_RST_PIN;               /* reset pulse */
    delay_ms(1U);
    PA->PORT_OUTSET = 1UL << OLED_RST_PIN;
    delay_ms(1U);
    /* MikroElektronika's oledc set-up */
    oled_command1(0xFD, 0x12);          /* unlock */
    oled_command1(0xFD, 0xB1);          /* and the locked commands */
    oled_command(0xAE, NULL, 0U);       /* sleep */
    oled_command1(0xB3, 0xF1);          /* clock */
    oled_command1(0xCA, 0x5F);          /* MUX 96 */
    oled_command1(0xA0, 0x32);          /* re-map, 65k colours */
    oled_command1(0xA1, 0x00);          /* start line */
    oled_command1(0xA2, 0x20);          /* offset: COM32.. */
    oled_command1(0xB5, 0x00);          /* GPIO */
    oled_command1(0xAB, 0x01);          /* internal VDD */
    oled_command1(0xB1, 0x32);          /* pre-charge */
    oled_command1(0xBE, 0x05);          /* VCOMH */
    oled_command(0xA6, NULL, 0U);       /* normal display */
    oled_command(0xC1, contrast, 3U);
    oled_command1(0xC7, 0x0F);          /* master contrast */
    oled_command(0xB4, vsl, 3U);
    oled_command1(0xB6, 0x01);          /* second pre-charge */
    oled_command(0xAF, NULL, 0U);       /* on */
}

/* Fill the rectangle (@x0, @y0)-(@x1, @y1) with @color */
static void fill(int x0, int y0, int x1, int y1, uint16_t color)
{
    uint8_t cols[2], rows[2];
    uint32_t n;

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 >= OLED_SIZE) { x1 = OLED_SIZE - 1; }
    if (y1 >= OLED_SIZE) { y1 = OLED_SIZE - 1; }
    if (x1 < x0 || y1 < y0)
    {
        return;
    }
    cols[0] = (uint8_t)(OLED_COL0 + (unsigned int)x0);
    cols[1] = (uint8_t)(OLED_COL0 + (unsigned int)x1);
    rows[0] = (uint8_t)y0;
    rows[1] = (uint8_t)y1;
    oled_command(0x15, cols, 2U);
    oled_command(0x75, rows, 2U);
    n = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);
    PA->PORT_OUTCLR = 1UL << OLED_CS_PIN;
    PB->PORT_OUTCLR = 1UL << OLED_DC_PIN;
    spi_byte(0x5C);                     /* write RAM */
    PB->PORT_OUTSET = 1UL << OLED_DC_PIN;
    while (n-- > 0U)
    {
        spi_byte((uint8_t)(color >> 8));
        spi_byte((uint8_t)color);
    }
    PA->PORT_OUTSET = 1UL << OLED_CS_PIN;
}

static void plot(int x, int y, uint16_t color)
{
    fill(x, y, x, y, color);
}

/* A 5x7 glyph at (@x, @y), each pixel @s x @s */
static void glyph(const uint8_t *g, int x, int y, int s, uint16_t color)
{
    for (int c = 0; c < 5; c++)
    {
        for (int r = 0; r < 7; r++)
        {
            if (((g[c] >> r) & 1U) != 0U)
            {
                fill(x + (c * s), y + (r * s), x + (c * s) + s - 1,
                     y + (r * s) + s - 1, color);
            }
        }
    }
}

static void line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;

    for (;;)
    {
        int e2 = 2 * err;               /* taken once per step */

        plot(x0, y0, color);
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

/* The point at radius @r and 6 x @i degrees clockwise from twelve o'clock */
static int dial_x(int r, int i)
{
    return DIAL_X + ((r * sin6[i % 60]) / 1000);
}

static int dial_y(int r, int i)
{
    return DIAL_Y - ((r * sin6[(i + 15) % 60]) / 1000);
}

/* The parts that never change: "ANGLE" and the dial */
static void draw_frame(void)
{
    fill(0, 0, OLED_SIZE - 1, OLED_SIZE - 1, BLACK);
    for (int i = 0; i < 5; i++)
    {
        glyph(font_angle[i], 3 + (i * 6), 3, 1, GREY);
    }
    for (int i = 0; i < 60; i++)
    {
        plot(dial_x(DIAL_R, i), dial_y(DIAL_R, i), CYAN);
    }
    for (int i = 0; i < 60; i += 15)                    /* N, E, S, W */
    {
        line(dial_x(DIAL_R - 4, i), dial_y(DIAL_R - 4, i),
             dial_x(DIAL_R, i), dial_y(DIAL_R, i), CYAN);
    }
}

static void draw_needle(int angle, uint16_t color)
{
    int i = angle / 6;                  /* angles are multiples of 12 */

    line(DIAL_X, DIAL_Y, dial_x(NEEDLE, i), dial_y(NEEDLE, i), color);
    fill(DIAL_X - 1, DIAL_Y - 1, DIAL_X + 1, DIAL_Y + 1, WHITE);
}

static void draw_digits(int angle)
{
    char digits[4];
    int n = 0, x = 4;

    fill(0, 13, OLED_SIZE - 1, 36, BLACK);
    do
    {
        digits[n++] = (char)('0' + (angle % 10));
        angle /= 10;
    } while (angle > 0);
    while (n > 0)
    {
        glyph(font_digits[digits[--n] - '0'], x, 14, 3, YELLOW);
        x += 18;
    }
    glyph(font_degree, x - 1, 13, 2, YELLOW);
}

static void show_angle(int angle, int before)
{
    char text[24] = "angle ";
    int n = 6, a = angle, k = 0;
    char rev[4];

    draw_digits(angle);
    if (before >= 0)
    {
        draw_needle(before, BLACK);
    }
    draw_needle(angle, RED);
    /* LED9 at twelve o'clock; 22.5 degrees a LED, clockwise */
    ring_show((uint16_t)(1U << ((8U + (((unsigned int)angle * 2U + 22U) /
                                       45U)) % 16U)));
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
// Section: Encoder, counted in the EIC interrupt (both edges of A), so a
// redraw never makes it miss a step
// *****************************************************************************

static volatile int detents;
static unsigned int enc_a;

/* Each detent move holds one edge of A: clockwise when A and B then differ */
void EIC_Handler(void)
{
    unsigned int a, b;

    EIC_REGS->EIC_INTFLAG = EIC_REGS->EIC_INTFLAG;
    a = (PB->PORT_IN >> ENC_A_PIN) & 1U;
    b = (PA->PORT_IN >> ENC_B_PIN) & 1U;
    if (a != enc_a)
    {
        detents += (a != b) ? 1 : -1;
        enc_a = a;
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
    /* PB22 = EXTINT6 (A): function A, input on; PA08 (B): a plain input */
    pin_function(PB, ENC_A_PIN, 0U);
    PB->PORT_PINCFG[ENC_A_PIN] |= PORT_PINCFG_INEN_Msk;
    PA->PORT_PINCFG[ENC_B_PIN] = PORT_PINCFG_INEN_Msk;
    EIC_REGS->EIC_CTRLA = 0U;
    while (EIC_REGS->EIC_SYNCBUSY != 0U)
    {
    }
    /* CONFIG0 holds EXTINT0..7, 4 bits each: SENSE = 3, both edges */
    EIC_REGS->EIC_CONFIG0 = 3UL << (ENC_A_EXTINT * 4U);
    EIC_REGS->EIC_INTENSET = 1UL << ENC_A_EXTINT;
    EIC_REGS->EIC_CTRLA = EIC_CTRLA_ENABLE_Msk;
    while (EIC_REGS->EIC_SYNCBUSY != 0U)
    {
    }
    enc_a = (PB->PORT_IN >> ENC_A_PIN) & 1U;
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
    uart_puts("Rotary angle on an OLED C: OLED C on EXT1, Rotary R on EXT2\r\n");
    spi_init();
    ring_init();
    oled_init();
    draw_frame();
    encoder_init();

    while ( true )
    {
        unsigned int sw = (PA->PORT_IN >> ENC_SW_PIN) & 1U;
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
            show_angle(angle, shown);
            shown = angle;
        }
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/
