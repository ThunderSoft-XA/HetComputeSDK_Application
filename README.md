# HetComputeSDK_Application
Basic Information: This is a Snapdragon Heterogeneous ComputeSDK for control CPU GPU and DSP

PlatformInfo: MSM8998, MSM8996, SDM660, SDM845.

Host BuildInfo: Unbuntu 18.04LTS

Maintainer: Shen Tao, Yang Rong

Start date: june 10th, 2019.

Function description: Application will create some pipes for control CPU, GPU, DSP processores.
                      We need to push task to signal processor or mutil processors, Application will show processor work time.
                      We can check it that some different about processor performance.

Application Sourcecode folder introduction:
1. SnapdragonHeterogeneousComputeSDK/1.0.0: This is a total folder and it include all applicatione and library files.
2. Application: This is a prebuild application for your test.
3. external: It include all external library for build HetCompute application and running.
4. include/hetcompute: This are some header files and from HetComputeSDK for build HetCompute application
5. lib: This is a prebuild library (libhetCompute-1.0.0.so), It will provide some API for application.
6. samples: It include application source and build script.

Application build and running:

Build:
1. Download android-ndk-r14b and set environment variable: export ANDROID_NDK=/home/<account>/Qualcomm_Demo/android-ndk-r14b
2. Download HetComputeSDK_demo code and set environment variable: 
   export  HETCOMPUTE_DIR=/home/<account>/Qualcomm_Demo/Qualcomm/SnapdragonHeterogeneousComputeSDK/1.0.0
3. Enter "SnapdragonHeterogeneousComputeSDK/1.0.0/samples/build/android/jni/" folder and run command: &ANDROID_NDK/ndk-build
4. Build out: Success build will create out application and library to libs.
              libs: SnapdragonHeterogeneousComputeSDK/1.0.0/samples/build/android/libs
                    It will include 32 and 64 bit application and library.(64-bit: arm64-v8a)(32-bit: armeabi-v7a)

Running:
1. First please install "HexagonSDK_for_HetComputeSDK_Demo" and run HetCompute_dsp_walkthrough_32bit.py or HetCompute_dsp_walkthrough_64bit.py script for install DSP control access.
2. Copy your application and library to DUT(Device unit test)
   hetcompute_sample_buffe_kernel_matrix_CPU_GPU_DSP: copy the application to "/data/local/tmp"
   libhetCompute-1.0.0.so: push it to "/vendor/lib" or "/vendor/lib64". (Follow your device attribute)
   libhetcompute_dsp.so: Ignore the library.
   libOpenCL.so: Qualcomm baseline has included the library. Ignore the library.
