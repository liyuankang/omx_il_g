
release libs are put here.

* to create the c-model and sw lib:
  $ cd ${VCBASE}\system
  $ make clean libclean vahevc
  $ make clean libclean vah264

* lib explain
  * libh2enc.a : sw lib
  * ench2_asic_model_h264.a : c-model lib for h264 and jpeg
  * ench2_asic_model_hevc.a : c-model lib for hevc and jpeg
