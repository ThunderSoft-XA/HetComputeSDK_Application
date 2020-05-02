# Copyright (c) 2017 Qualcomm Technologies, Inc.
# Qualcomm® Snapdragon™ Heterogeneous Compute SDK Examples

# This is the folder containing Android.mk
LOCAL_PATH := $(call my-dir)

# Heterogeneous Compute SDK version
QSHETCOMPUTE_VERSION = 1.0.0

# paths (can be overridden on the cmd line)
QSHETCOMPUTE_SAMPLES_SRC_PATH := $(LOCAL_PATH)/../../../src

QSHETCOMPUTE_CORE_INCLUDE_PATH := $(LOCAL_PATH)/../../../../include
QSHETCOMPUTE_CORE_LIB_PATH := $(LOCAL_PATH)/../../../../lib
QSHETCOMPUTE_DSP_STUB_PATH := $(LOCAL_PATH)/../../../../external/dsp

# OpenCL Paths
QSHETCOMPUTE_OPENCL_PATH := $(LOCAL_PATH)/../../../../external/opencl-sdk-1.2.2
QSHETCOMPUTE_OPENCL_INC_PATH   := $(QSHETCOMPUTE_OPENCL_PATH)/inc/
QSHETCOMPUTE_OPENCL_LIB32_PATH := $(QSHETCOMPUTE_OPENCL_PATH)/lib
QSHETCOMPUTE_OPENCL_LIB64_PATH := $(QSHETCOMPUTE_OPENCL_PATH)/lib64

# Hexagon SDK and Hetcompute-DSP-Stub Paths
QSHETCOMPUTE_DSP_STUB_INC_PATH := $(QSHETCOMPUTE_DSP_STUB_PATH)/include
QSHETCOMPUTE_DSP_STUB_LIB32_PATH := $(QSHETCOMPUTE_DSP_STUB_PATH)/lib
QSHETCOMPUTE_DSP_STUB_LIB64_PATH := $(QSHETCOMPUTE_DSP_STUB_PATH)/lib64

###############################################################################
# OpenCL Library
include $(CLEAR_VARS)

LOCAL_MODULE := libOpenCL-prebuilt
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_SRC_FILES := $(QSHETCOMPUTE_OPENCL_LIB32_PATH)/libOpenCL.so
else
    LOCAL_SRC_FILES := $(QSHETCOMPUTE_OPENCL_LIB64_PATH)/libOpenCL.so
endif
LOCAL_EXPORT_C_INCLUDES = $(QSHETCOMPUTE_OPENCL_INC_PATH)/

include $(PREBUILT_SHARED_LIBRARY)

###############################################################################
# Hexagon SDK - libhexagon
include $(CLEAR_VARS)

LOCAL_MODULE := libhetcompute-hexagon-prebuilt

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_SRC_FILES := $(QSHETCOMPUTE_DSP_STUB_LIB32_PATH)/libhetcompute_dsp.so
else
    LOCAL_SRC_FILES := $(QSHETCOMPUTE_DSP_STUB_LIB64_PATH)/libhetcompute_dsp.so
endif

include $(PREBUILT_SHARED_LIBRARY)

###############################################################################
# Heterogeneous Compute SDK prebuilt
include $(CLEAR_VARS)

LOCAL_MODULE := qshetcompute
LOCAL_SRC_FILES := $(QSHETCOMPUTE_CORE_LIB_PATH)/$(TARGET_ARCH_ABI)/libhetCompute-$(QSHETCOMPUTE_VERSION).so
LOCAL_EXPORT_C_INCLUDES := $(QSHETCOMPUTE_CORE_INCLUDE_PATH)

include $(PREBUILT_SHARED_LIBRARY)

###############################################################################

define hetcompute_add_sample
  include $(CLEAR_VARS)
  LOCAL_MODULE := hetcompute_sample_$1
  LOCAL_C_INCLUDES := $(QSHETCOMPUTE_CORE_INCLUDE_PATH) \
                      $(QSHETCOMPUTE_OPENCL_INC_PATH) \
                      $(QSHETCOMPUTE_DSP_STUB_PATH)

  LOCAL_SHARED_LIBRARIES := qshetcompute libhetcompute-hexagon-prebuilt libOpenCL-prebuilt
  LOCAL_CPPFLAGS := -pthread -std=c++11 -stdlib=libstdc++
  LOCAL_LDLIBS := -llog -lGLESv3 -lEGL
  LOCAL_CFLAGS := -DHAVE_CONFIG_H=1 -DHAVE_ANDROID_LOG_H=1 -DHETCOMPUTE_HAVE_RTTI=1 -DHETCOMPUTE_HAVE_OPENCL=1 -DHETCOMPUTE_HAVE_GPU=1 -DHETCOMPUTE_HAVE_GLES=1 -DHETCOMPUTE_HAVE_QTI_DSP=1 -DHETCOMPUTE_THROW_ON_API_ASSERT=1 -DHETCOMPUTE_LOG_FIRE_EVENT=1
  ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
    LOCAL_LDFLAGS := -Wl,-allow-shlib-undefined
  endif
  LOCAL_SRC_FILES := $(QSHETCOMPUTE_SAMPLES_SRC_PATH)/$1.cc
  include $(BUILD_EXECUTABLE)
endef

###############################################################################

sample_names :=  \
  MatrixAlgorithmDemo \
  ImageProcessingDemo \
  ParallelTaskDependencyDemo \
  ParallelPatternsDemo

###############################################################################

$(foreach ex,$(sample_names),$(eval $(call hetcompute_add_sample,$(ex))))
