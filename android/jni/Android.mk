LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

INC=../../include
SRC=../../src
TST=../../tests

LOCAL_MODULE    := ncine
LOCAL_CFLAGS    := -Werror
LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(INC)
LOCAL_SRC_FILES := main.cpp \
	$(SRC)/base/ncVector2f.cpp \
	$(SRC)/ncServiceLocator.cpp \
	$(SRC)/ncFileLogger.cpp \
	$(SRC)/ncTimer.cpp \
	$(SRC)/ncFrameTimer.cpp \
	$(SRC)/ncArrayIndexer.cpp \
	$(SRC)/ncProfileVariable.cpp \
	$(SRC)/ncApplication.cpp \
	$(SRC)/ncFont.cpp \
	$(SRC)/graphics/ncIGfxDevice.cpp \
	$(SRC)/graphics/ncEGLGfxDevice.cpp \
	$(SRC)/graphics/ncTextureFormat.cpp \
	$(SRC)/graphics/ncTextureLoader.cpp \
	$(SRC)/graphics/ncTexture.cpp \
	$(SRC)/graphics/ncProfilePlotter.cpp \
	$(SRC)/graphics/ncLinePlotter.cpp \
	$(SRC)/graphics/ncStackedBarPlotter.cpp \
	$(SRC)/graphics/ncSceneNode.cpp \
	$(SRC)/graphics/ncSprite.cpp \
	$(SRC)/graphics/ncRenderCommand.cpp \
	$(SRC)/graphics/ncRenderQueue.cpp \
	$(SRC)/graphics/ncSpriteBatchNode.cpp \
	$(SRC)/graphics/ncParticle.cpp \
	$(SRC)/graphics/ncParticleAffectors.cpp \
	$(SRC)/graphics/ncParticleSystem.cpp \
	$(SRC)/graphics/ncTextNode.cpp \
	$(TST)/test_particlesapp.cpp

LOCAL_LDLIBS    := -lm -llog -landroid -lEGL -lGLESv1_CM
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
