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
