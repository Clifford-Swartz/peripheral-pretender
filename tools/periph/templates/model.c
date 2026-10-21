/*
 * Microchip $MODULE ($IP)
 *
 * Peripheral Pretender: a personal project. AI-assisted code;
 * not for submission to upstream QEMU.
 *
 * What the $DEVICE (firmware/hello APP=${name}probe) showed, and the model
 * does:
 *
 *   - ...
 *
 * Not modelled: ...
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "hw/core/irq.h"
#include "hw/misc/mchp_${name}.h"
#include "hw/mchp/gen/${module}_$ip.h"
#include "migration/vmstate.h"

OBJECT_DECLARE_SIMPLE_TYPE($STATE, $CAST)

struct $STATE {
    MchpRegBlock parent_obj;
};

static uint32_t reg($STATE *s, hwaddr off, unsigned size)
{
    return mchp_regfile_peek(&MCHP_REGBLOCK(s)->rf, off, size);
}

static void set_reg($STATE *s, hwaddr off, unsigned size, uint32_t v)
{
    mchp_regfile_poke(&MCHP_REGBLOCK(s)->rf, off, size, v);
}

static uint32_t pre_read(MchpRegFile *rf, const MchpRegInfo *ri,
                         unsigned idx, uint32_t stored, bool side_effects)
{
    return stored;
}

static void post_write(MchpRegFile *rf, const MchpRegInfo *ri, unsigned idx,
                       uint32_t old, uint32_t written, uint32_t we)
{
    $STATE *s = $CAST(rf->opaque);

    (void)s;
    (void)reg;
    (void)set_reg;
}

static void reset_hold(MchpRegBlock *rb)
{
}

static void ${name}_init(Object *obj)
{
}

static const VMStateDescription vmstate_${name} = {
    .name = $TYPE,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_STRUCT(parent_obj, $STATE, 1, vmstate_mchp_regblock,
                       MchpRegBlock),
        VMSTATE_END_OF_LIST()
    },
};

static void ${name}_class_init(ObjectClass *klass, const void *data)
{
    MchpRegBlockClass *c = MCHP_REGBLOCK_CLASS(klass);

    DEVICE_CLASS(klass)->vmsd = &vmstate_${name};
    c->pre_read = pre_read;
    c->post_write = post_write;
    c->reset_hold = reset_hold;
    c->log_writes = false;
}

static const TypeInfo ${name}_types[] = {
    {
        .name = $TYPE,
        .parent = TYPE_MCHP_REGBLOCK,
        .instance_size = sizeof($STATE),
        .instance_init = ${name}_init,
        .class_init = ${name}_class_init,
    },
};

DEFINE_TYPES(${name}_types)
