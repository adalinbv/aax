/*
 * Copyright 2005-2013 by Erik Hofman.
 * Copyright 2009-2013 by Adalin B.V.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Adalin B.V.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Adalin B.V.
 */

#ifndef _AAX_RBUF_EFFECTS3D_H
#define _AAX_RBUF_EFFECTS3D_H 1

#if defined(__cplusplus)
extern "C" {
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "software/cpu2d/rbuf_effects.h"

enum
{
    /* 3d filters */
    DISTANCE_FILTER = 0,        /* distance attennuation */
    ANGULAR_FILTER,             /* audio cone support    */
    MAX_3D_FILTER,

    /* 3d effects */
    VELOCITY_EFFECT = 0,        /* Doppler               */
    MAX_3D_EFFECT,
};

/* 3d properties */
#define _PROP3D_CLEAR(q)                ((q)->state3d &= (CONE_DEFINED|DYNAMIC_PITCH_DEFINED))
#define _PROP3D_PITCH_HAS_CHANGED(q)    ((q)->state3d & PITCH_CHANGE)
#define _PROP3D_GAIN_HAS_CHANGED(q)     ((q)->state3d & GAIN_CHANGED)
#define _PROP3D_DIST_HAS_CHANGED(q)     ((q)->state3d & DIST_CHANGED)
#define _PROP3D_MTX_HAS_CHANGED(q)      ((q)->state3d & MTX_CHANGED)
#define _PROP3D_SPEED_HAS_CHANGED(q)    ((q)->state3d & SPEED_CHANGED)
#define _PROP3D_MTXSPEED_HAS_CHANGED(q) ((q)->state3d & (SPEED_CHANGED|MTX_CHANGED))
#define _PROP3D_CONE_IS_DEFINED(q)      ((q)->state3d & CONE_DEFINED)

#define _PROP3D_PITCH_SET_CHANGED(q)    ((q)->state3d |= PITCH_CHANGED)
#define _PROP3D_GAIN_SET_CHANGED(q)     ((q)->state3d |= GAIN_CHANGED)
#define _PROP3D_DIST_SET_CHANGED(q)     ((q)->state3d |= DIST_CHANGED)
#define _PROP3D_MTX_SET_CHANGED(q)      ((q)->state3d |= MTX_CHANGED)
#define _PROP3D_SPEED_SET_CHANGED(q)    ((q)->state3d |= SPEED_CHANGED)
#define _PROP3D_CONE_SET_DEFINED(q)     ((q)->state3d |= CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_SET_DEFINED(q) ((q)->state3d |= DYNAMIC_PITCH_DEFINED)

#define _PROP3D_PITCH_CLEAR_CHANGED(q)  ((q)->state3d &= ~PITCH_CHANGED)
#define _PROP3D_GAIN_CLEAR_CHANGED(q)   ((q)->state3d &= ~GAIN_CHANGED)
#define _PROP3D_DIST_CLEAR_CHANGED(q)   ((q)->state3d &= ~DIST_CHANGED)
#define _PROP3D_MTX_CLEAR_CHANGED(q)    ((q)->state3d &= ~MTX_CHANGED)
#define _PROP3D_SPEED_CLEAR_CHANGED(q)  ((q)->state3d &= ~SPEED_CHANGED)
#define _PROP3D_CONE_CLEAR_DEFINED(q)   ((q)->state3d &= ~CONE_DEFINED)
#define _PROP3D_DYNAMIC_PITCH_CLEAR_DEFINED(q) ((q)->state3d &= ~DYNAMIC_PITCH_DEFINED)

/* 3d properties: AAX Scene extension*/
#define _PROP3D_SCENE_IS_DEFINED(q)     ((q)->state3d & SCENE_CHANGED)
#define _PROP3D_REVERB_IS_DEFINED(q)    ((q)->state3d & REVERB_CHANGED)
#define _PROP3D_DISTDELAY_IS_DEFINED(q) ((q)->state3d & DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_IS_DEFINED(q) ((q)->state3d & DISTQUEUE_CHANGED)
#define _PROP3D_WIND_IS_DEFINED(q)      ((q)->state3d & WIND_CHANGED)

#define _PROP3D_SCENE_SET_CHANGED(q)    ((q)->state3d |= SCENE_CHANGED)
#define _PROP3D_REVERB_SET_CHANGED(q)   ((q)->state3d |= REVERB_CHANGED)
#define _PROP3D_DISTDELAY_SET_DEFINED(q) ((q)->state3d |= DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_SET_DEFINED(q) ((q)->state3d |= (DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_SET_CHANGED(q)     ((q)->state3d |= WIND_CHANGED)

#define _PROP3D_SCENE_CLEAR_CHANGED(q)  ((q)->state3d &= ~SCENE_CHANGED)
#define _PROP3D_REVERB_CLEAR_CHANGED(q) ((q)->state3d &= ~REVERB_CHANGED)
#define _PROP3D_DISTDELAY_CLEAR_DEFINED(q) ((q)->state3d &= ~DISTDELAY_CHANGED)
#define _PROP3D_DISTQUEUE_CLEAR_DEFINED(q) ((q)->state3d &= ~(DISTQUEUE_CHANGED|DISTDELAY_CHANGED))
#define _PROP3D_WIND_CLEAR_CHANGED(q)   ((q)->state3d &= ~WIND_CHANGED)


typedef float _aaxRingBufferDistFn(float, float, float, float, float, float);

/* -------------------------------------------------------------------------- */

extern _aaxRingBufferDistFn* _aaxRingBufferDistanceFn[];
extern _aaxRingBufferDistFn* _aaxRingBufferALDistanceFn[];

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* !_AAX_RBUF_EFFECTS3D_H */

