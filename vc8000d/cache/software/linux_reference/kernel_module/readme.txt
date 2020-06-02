/**************************Verisilicon Cache&Shaper Readme**************************/
1. In order to port cache&shaper driver to your system, please modify porting layer in hantro_cache.c mainly in
   core_array[] which customer can specify which cores should support and the base addr and int_pin of each core.
   
2. This driver can be built to run either in linux PC with PCIE or irq mode in ARM environment by configuring a 
   defination PCI_BUS in hanro_cache.h. By default, PCI bus is defined.