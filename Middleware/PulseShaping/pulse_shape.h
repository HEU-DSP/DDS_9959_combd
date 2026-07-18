/**
 ******************************************************************************
 * @file    pulse_shape.h
 * @brief   Pulse Shaping Filters — Skeleton (Phase 5)
 ******************************************************************************
 */

#ifndef __PULSE_SHAPE_H__
#define __PULSE_SHAPE_H__

#include <stdint.h>

typedef enum {
    PULSE_GAUSSIAN = 0,
    PULSE_RRC,
    PULSE_HALF_SINE,
} PulseType;

void PulseShape_Init(PulseType type, float bt, int samples_per_sym);
void PulseShape_Apply(const float *input, float *output, int len);

#endif /* __PULSE_SHAPE_H__ */
