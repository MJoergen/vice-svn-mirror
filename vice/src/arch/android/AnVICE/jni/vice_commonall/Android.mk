LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := vice_common

MY_PATH2 := ../../../../../..

CG_SUBDIRS := \
$(MY_PATH2)/src/sid \
$(MY_PATH2)/src/raster \
$(MY_PATH2)/src/video \
$(MY_PATH2)/src/monitor \
$(MY_PATH2)/src/core \
$(MY_PATH2)/src/platform

# Add more subdirs here, like src/subdir1 src/subdir2

MY_PATH := $(LOCAL_PATH)

CG_SRCDIR := $(LOCAL_PATH)

LOCAL_CFLAGS := -DANDROID_COMPILE

LOCAL_C_INCLUDES :=	$(LOCAL_PATH)/include \
				$(LOCAL_PATH)/../sdl/include \
				$(LOCAL_PATH)/../locnet/include \
				$(LOCAL_PATH) \
				$(LOCAL_PATH)/../../../../../../src/vdrive \
				$(LOCAL_PATH)/../../../../../../src/drive \
				$(LOCAL_PATH)/../../../../../../src/raster \
				$(LOCAL_PATH)/../../../../../../src/rs232drv \
				$(LOCAL_PATH)/../../../../../../src/c64 \
				$(LOCAL_PATH)/../../../../../../src/sid \
				$(LOCAL_PATH)/../../../../../../src/platform \
				$(LOCAL_PATH)/../../../../../../src/drive \
				$(LOCAL_PATH)/../../../../../../src/lib/p64 \
				$(LOCAL_PATH)/../../../../../../src/userport \
				$(LOCAL_PATH)/../../../../../../src/plus4 \
				$(LOCAL_PATH)/../../../../../../src/vic20 \
				$(LOCAL_PATH)/../../../../../../src/arch/sdl \
				$(LOCAL_PATH)/../../../../../../src/arch \
				$(LOCAL_PATH)/../../../../../../src \
				$(LOCAL_PATH)/../.. \
				$(LOCAL_PATH)

LOCAL_PATH := $(MY_PATH)

LOCAL_CPPFLAGS := $(LOCAL_CFLAGS)
LOCAL_CXXFLAGS := $(LOCAL_CFLAGS)

#Change C++ file extension as appropriate
LOCAL_CPP_EXTENSION := .cpp

#LOCAL_SRC_FILES := $(foreach F, $(CG_SUBDIRS), $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.cpp))))
# Uncomment to also add C sources
#LOCAL_SRC_FILES += $(foreach F, $(CG_SUBDIRS), $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.c))))
LOCAL_SRC_FILES += \
	$(MY_PATH2)/src/arch/sdl/archdep.c \
	$(MY_PATH2)/src/arch/sdl/blockdev.c \
	$(MY_PATH2)/src/arch/sdl/catweaselmkiii.c \
	$(MY_PATH2)/src/arch/sdl/console.c \
	$(MY_PATH2)/src/arch/sdl/coproc.c \
	$(MY_PATH2)/src/arch/sdl/dynlib.c \
	$(MY_PATH2)/src/arch/sdl/fullscreen.c \
	$(MY_PATH2)/src/arch/sdl/hardsid.c \
	$(MY_PATH2)/src/arch/sdl/joy.c \
	$(MY_PATH2)/src/arch/sdl/kbd.c \
	$(MY_PATH2)/src/arch/sdl/lightpendrv.c \
	$(MY_PATH2)/src/arch/sdl/menu_c64cart.c \
	$(MY_PATH2)/src/arch/sdl/menu_c64_common_expansions.c \
	$(MY_PATH2)/src/arch/sdl/menu_c64_expansions.c \
	$(MY_PATH2)/src/arch/sdl/menu_c64hw.c \
	$(MY_PATH2)/src/arch/sdl/menu_c64model.c \
	$(MY_PATH2)/src/arch/sdl/menu_common.c \
	$(MY_PATH2)/src/arch/sdl/menu_debug.c \
	$(MY_PATH2)/src/arch/sdl/menu_drive.c \
	$(MY_PATH2)/src/arch/sdl/menu_drive_rom.c \
	$(MY_PATH2)/src/arch/sdl/menu_ffmpeg.c \
	$(MY_PATH2)/src/arch/sdl/menu_help.c \
	$(MY_PATH2)/src/arch/sdl/menu_joystick.c \
	$(MY_PATH2)/src/arch/sdl/menu_lightpen.c \
	$(MY_PATH2)/src/arch/sdl/menu_midi.c \
	$(MY_PATH2)/src/arch/sdl/menu_mouse.c \
	$(MY_PATH2)/src/arch/sdl/menu_network.c \
	$(MY_PATH2)/src/arch/sdl/menu_printer.c \
	$(MY_PATH2)/src/arch/sdl/menu_ram.c \
	$(MY_PATH2)/src/arch/sdl/menu_reset.c \
	$(MY_PATH2)/src/arch/sdl/menu_rom.c \
	$(MY_PATH2)/src/arch/sdl/menu_rs232.c \
	$(MY_PATH2)/src/arch/sdl/menu_screenshot.c \
	$(MY_PATH2)/src/arch/sdl/menu_settings.c \
	$(MY_PATH2)/src/arch/sdl/menu_sid.c \
	$(MY_PATH2)/src/arch/sdl/menu_snapshot.c \
	$(MY_PATH2)/src/arch/sdl/menu_sound.c \
	$(MY_PATH2)/src/arch/sdl/menu_speed.c \
	$(MY_PATH2)/src/arch/sdl/menu_tape.c \
	$(MY_PATH2)/src/arch/sdl/menu_tfe.c \
	$(MY_PATH2)/src/arch/sdl/menu_video.c \
	$(MY_PATH2)/src/arch/sdl/mididrv.c \
	$(MY_PATH2)/src/arch/sdl/mousedrv.c \
	$(MY_PATH2)/src/arch/sdl/parsid.c \
	$(MY_PATH2)/src/arch/sdl/rs232.c \
	$(MY_PATH2)/src/arch/sdl/rs232dev.c \
	$(MY_PATH2)/src/arch/sdl/rs232net.c \
	$(MY_PATH2)/src/arch/sdl/sdlmain.c \
	$(MY_PATH2)/src/arch/sdl/signals.c \
	$(MY_PATH2)/src/arch/sdl/rawnetarch.c \
	$(MY_PATH2)/src/arch/sdl/ui.c \
	$(MY_PATH2)/src/arch/sdl/uicmdline.c \
	$(MY_PATH2)/src/arch/sdl/uifilereq.c \
	$(MY_PATH2)/src/arch/sdl/uihotkey.c \
	$(MY_PATH2)/src/arch/sdl/uimenu.c \
	$(MY_PATH2)/src/arch/sdl/uimon.c \
	$(MY_PATH2)/src/arch/sdl/uimsgbox.c \
	$(MY_PATH2)/src/arch/sdl/uipause.c \
	$(MY_PATH2)/src/arch/sdl/uipoll.c \
	$(MY_PATH2)/src/arch/sdl/uistatusbar.c \
	$(MY_PATH2)/src/arch/sdl/video.c \
	$(MY_PATH2)/src/arch/sdl/vkbd.c \
	$(MY_PATH2)/src/arch/sdl/vsidui.c \
	$(MY_PATH2)/src/arch/sdl/vsyncarch.c \
	$(MY_PATH2)/src/arch/sdl/x64_ui.c \
	$(MY_PATH2)/src/arch/sdl/x64sc_ui.c \
	$(MY_PATH2)/src/serial/fsdrive.c \
	$(MY_PATH2)/src/serial/serial.c \
	$(MY_PATH2)/src/serial/serial-device.c \
	$(MY_PATH2)/src/serial/serial-iec.c \
	$(MY_PATH2)/src/serial/serial-iec-bus.c \
	$(MY_PATH2)/src/serial/serial-iec-device.c \
	$(MY_PATH2)/src/serial/serial-iec-lib.c \
	$(MY_PATH2)/src/serial/serial-realdevice.c \
	$(MY_PATH2)/src/serial/serial-trap.c \
	$(MY_PATH2)/src/alarm.c \
	$(MY_PATH2)/src/attach.c \
	$(MY_PATH2)/src/autostart.c \
	$(MY_PATH2)/src/autostart-prg.c \
	$(MY_PATH2)/src/charset.c \
	$(MY_PATH2)/src/clkguard.c \
	$(MY_PATH2)/src/clipboard.c \
	$(MY_PATH2)/src/cmdline.c \
	$(MY_PATH2)/src/cbmdos.c \
	$(MY_PATH2)/src/cbmimage.c \
	$(MY_PATH2)/src/color.c \
	$(MY_PATH2)/src/crc32.c \
	$(MY_PATH2)/src/datasette.c \
	$(MY_PATH2)/src/debug.c \
	$(MY_PATH2)/src/dma.c \
	$(MY_PATH2)/src/embedded.c \
	$(MY_PATH2)/src/event.c \
	$(MY_PATH2)/src/findpath.c \
	$(MY_PATH2)/src/fliplist.c \
	$(MY_PATH2)/src/gcr.c \
	$(MY_PATH2)/src/info.c \
	$(MY_PATH2)/src/init.c \
	$(MY_PATH2)/src/initcmdline.c \
	$(MY_PATH2)/src/interrupt.c \
	$(MY_PATH2)/src/ioutil.c \
	$(MY_PATH2)/src/joystick.c \
	$(MY_PATH2)/src/kbdbuf.c \
	$(MY_PATH2)/src/keyboard.c \
	$(MY_PATH2)/src/lib.c \
	$(MY_PATH2)/src/libm_math.c \
	$(MY_PATH2)/src/lightpen.c \
	$(MY_PATH2)/src/log.c \
	$(MY_PATH2)/src/machine-bus.c \
	$(MY_PATH2)/src/machine.c \
	$(MY_PATH2)/src/main.c \
	$(MY_PATH2)/src/network.c \
	$(MY_PATH2)/src/opencbmlib.c \
	$(MY_PATH2)/src/palette.c \
	$(MY_PATH2)/src/ram.c \
	$(MY_PATH2)/src/rawfile.c \
	$(MY_PATH2)/src/rawnet.c \
	$(MY_PATH2)/src/resources.c \
	$(MY_PATH2)/src/romset.c \
	$(MY_PATH2)/src/screenshot.c \
	$(MY_PATH2)/src/snapshot.c \
	$(MY_PATH2)/src/socket.c \
	$(MY_PATH2)/src/sound.c \
	$(MY_PATH2)/src/sysfile.c \
	$(MY_PATH2)/src/translate.c \
	$(MY_PATH2)/src/traps.c \
	$(MY_PATH2)/src/util.c \
	$(MY_PATH2)/src/vsync.c \
	$(MY_PATH2)/src/zfile.c \
	$(MY_PATH2)/src/zipcode.c \
	$(MY_PATH2)/src/mouse.c \
	$(MY_PATH2)/src/midi.c \
	$(MY_PATH2)/src/maincpu.c \
	$(MY_PATH2)/src/sounddrv/soundaiff.c \
	$(MY_PATH2)/src/sounddrv/sounddummy.c \
	$(MY_PATH2)/src/sounddrv/sounddump.c \
	$(MY_PATH2)/src/sounddrv/soundfs.c \
	$(MY_PATH2)/src/sounddrv/soundiff.c \
	$(MY_PATH2)/src/sounddrv/soundmovie.c \
	$(MY_PATH2)/src/sounddrv/soundsdl.c \
	$(MY_PATH2)/src/sounddrv/soundvoc.c \
	$(MY_PATH2)/src/sounddrv/soundwav.c
	
# LOCAL_STATIC_LIBRARIES := sdl_mixer sdl tremor stlport
#LOCAL_STATIC_LIBRARIES := locnet_al

#LOCAL_LDLIBS := -lGLESv1_CM -ldl -llog -lsupc++ -ljnigraphics

#LOCAL_LDLIBS := -ldl -lsupc++ -ljnigraphics

LOCAL_ARM_MODE := arm

include $(BUILD_STATIC_LIBRARY)
