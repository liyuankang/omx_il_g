#ifndef _SOFT_VP9_API_H
#define _SOFT_VP9_API_H

#include "hevcencapi.h"
#include "vp9_bitstream.h"
#include "vp9_prob.h"
#include "sw_picture.h"
#include "instance.h"

void VCEncInitVP9(const VCEncConfig *config, VCEncInst inst);
void VCEncGenVP9Config(VCEncInst inst, const VCEncIn *pEncIn, struct sw_picture *pic, VCEncPictureCodingType codingType);
void VCEncVP9SetBuffer(struct vcenc_instance *vcenc_instance, struct buffer *b);
void VCEncVP9ResetEntropy(struct vcenc_instance* vcenc_instance);

#endif