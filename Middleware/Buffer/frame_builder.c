/**
 ******************************************************************************
 * @file    frame_builder.c
 * @brief   SPI Frame Builder implementation
 ******************************************************************************
 */

#include "frame_builder.h"
#include "mod_cw.h"
#include "dds_encoder.h"

int FrameBuilder_CW(uint8_t *s1, uint8_t *s3,
                    uint32_t ftw, uint16_t asf, uint8_t profile)
{
    return CW_GenerateFrame(ftw, asf, profile, s1, s3);
}

int FrameBuilder_FSK(uint8_t *s1, uint8_t *s3,
                     const FSK_Config *cfg, uint8_t bit)
{
    return FSK_GenerateFrame(cfg, bit, s1, s3);
}

int FrameBuilder_ASK(uint8_t *s1, uint8_t *s3,
                     const ASK_Config *cfg, uint8_t bit)
{
    return ASK_GenerateFrame(cfg, bit, s1, s3);
}

int FrameBuilder_GFSK(uint8_t *s1, uint8_t *s3,
                      const GFSK_Config *cfg, uint8_t bit)
{
    return GFSK_GenerateFrame(cfg, bit, s1, s3);
}

int FrameBuilder_MSK(uint8_t *s1, uint8_t *s3,
                     const MSK_Config *cfg, uint8_t bit)
{
    return MSK_GenerateFrame(cfg, bit, s1, s3);
}

int FrameBuilder_QPSK(uint8_t *s1, uint8_t *s3,
                      const QPSK_Config *cfg, uint8_t symbol)
{
    return QPSK_GenerateFrame(cfg, symbol, s1, s3);
}

int FrameBuilder_AM(uint8_t *s1, uint8_t *s3,
                    const AM_Config *cfg, int16_t sample)
{
    return AM_GenerateFrame(cfg, sample, s1, s3);
}

int FrameBuilder_FM(uint8_t *s1, uint8_t *s3,
                    const FM_Config *cfg, int16_t sample)
{
    return FM_GenerateFrame(cfg, sample, s1, s3);
}

int FrameBuilder_BPSK(uint8_t *s1, uint8_t *s3,
                    const BPSK_Config *cfg, uint8_t bit)
{
    return BPSK_GenerateFrame(cfg, bit, s1, s3);
}

int FrameBuilder_4FSK(uint8_t *s1, uint8_t *s3,
                      const FSK4_Config *cfg, uint8_t symbol)
{
    return FSK4_GenerateFrame(cfg, symbol, s1, s3);
}
