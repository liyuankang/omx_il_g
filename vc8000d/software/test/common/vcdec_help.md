# Options in Testbench for VC8000D VIDEO
## Parameters  affecting encoder input/output frames

```
    -Z Run output handling in separate thread. (--separate-output-thread)  
    
     Enable hardware features (all listed features disabled by default)  
     
    -E<feature> (--enable), enable hw feature where <feature> is  
    
   one of the following:  
     rs -- raster scan conversion  
     
     p010 -- output in P010 format for 10-bit stream  
     
    -s[yc]NNN (--out_stride [yc]NNN), Set PP stride for y/c plane, sw check the validation of the value. E.g.,  
    -sy720 -sc360     Set ystride and cstride to 720 and 360.  
    
    -d<downscale> (--down_scale), enable down scale feature where <downscale> is one of the following:  
     <ds_ratio> -- down scale to 1/<ds_ratio> in both directions  
     <ds_ratio_x>:<ds_ratio_y> -- down scale to 1/<ds_ratio_x> in horizontal and 1/<ds_ratio_y> in vertical  
      <ds_ratio> should be one of following: 2, 4, 8  
      
    -Dwxh  PP output size wxh. E.g.,  
    -D1280x720        PP output size 1280x720  
    
    -C[xywh]NNN (--crop [xywh]NNN), Cropping parameters. E.g.,  
    -Cx8 -Cy16        Crop from (8, 16)  
    -Cw720 -Ch480     Crop size  720x480  
    -Cd Output crop picture by testbench instead of PP.  
    
    -f Force output in 8 bits per pixel for HEVC Main 10 profile. (--force-8bits)  
    
    --shaper_bypass Enable shaper bypass rtl simulation (external hw IP). 
     
    --cache_enable  Enable cache rtl simulation (external hw IP).  
    
    --shaper_enable Enable shaper rtl simulation (external hw IP).  
    
    --pp-shaper Enable shaper for ppu0.  
    
    --delogo(--enable), enable delogo feature, need configure parameters as the following:  
     pos -- the delog pos wxh@(x,y)  
     show -- show the delogo border  
     mode -- select the delogo mode  
     YUV -- set the replace value if use PIXEL_REPLACE mode
     
   Temporary testing multiple decoder instances in multi-thread mode or multi-process mode:
    --multimode     Specify decoders running in parallel in multi-thread or multi-process mode. [0]
                     0: disable
                     1: multi-thread mode
                     2: multi-process mode(next to do)
    --streamcfg    Specify the filename storing decoder options
    
     Other features:  
    -b Bypass reference frame compression (--compress-bypass)  
    -n Use non-ringbuffer mode for stream input buffer. Default: ring buffer mode. (--non-ringbuffer)  
    -A<n> Set stride aligned to n bytes (valid value: 1/8/16/32/64/128/256/512/1024/2048)  
    --ultra-low-latency Data transmission use low latency mode  
    --platform-low-latency Enable low latency platform running flag  
    --secure Enable secure mode flag  
    --tile-by-tile Enable tile-by-tile decoding  
    --cr-first PP outputs chroma in CrCb order  
    --mc enable Enable frame level multi-core decoding  
    --mvc Enable MVC decoding (H264 only).  
    --partial Enable partial decoding.  
    --pp-rgb Enable Yuv2RGB.  
    --pp-rgb-planar Enable planar Yuv2RGB.  
    --rgb-fmat set the RGB output format.  
    --rgb-std set the standard coeff to do Yuv2RGB.  
    --second-crop enable second crop configure.  
```
