x86 env:
1. To build g2dec:
	make clean g2dec USE_EXTERNAL_BUFFER=y RELEASE=y USE_64BIT_ENV=y USE_MODEL_LIB=./cmodel_lib/libvc8kd.a

2. To build h264dec:
	make clean h264dec USE_EXTERNAL_BUFFER=y RELEASE=y USE_64BIT_ENV=y USE_MODEL_LIB=./cmodel_lib/libvc8kd.a

3. To build jpegdec:
	make clean jpegdec USE_EXTERNAL_BUFFER=y RELEASE=y USE_64BIT_ENV=y USE_MODEL_LIB=./cmodel_lib/libvc8kd.a
output: out/x86_linux/release/jpegdec

arm env:

arm toolchain: gcc-linaro-7.1.1-2017.08-i686_aarch64-linux-gnu

1. Set arm toolchain location in PATH:
	export PATH=$PATH:/home/xx/gcc-linaro-7.1.1-2017.08-i686_aarch64-linux-gnu/bin

2. To build arm Jpegdec:
	make clean jpegdec ENV=arm_linux DEBUG=y USE_64BIT_ENV=y

3. To build arm g2dec:
	make clean g2dec ENV=arm_pclinux USE_EXTERNAL_BUFFER=y RELEASE=y USE_64BIT_ENV=y USE_MODEL_LIB=./cmodel_lib_arm64/libvc8kd.a
