add_library(circle_lib STATIC)
  
target_sources(circle_lib PRIVATE
  ${CIRCLE_DIR}/lib/actled.cpp
  ${CIRCLE_DIR}/lib/alloc.cpp
  ${CIRCLE_DIR}/lib/assert.cpp
  ${CIRCLE_DIR}/lib/bcmframebuffer.cpp
  ${CIRCLE_DIR}/lib/bcmmailbox.cpp
  ${CIRCLE_DIR}/lib/bcmpropertytags.cpp
  ${CIRCLE_DIR}/lib/cache-v7.S
  ${CIRCLE_DIR}/lib/chainboot.cpp
  ${CIRCLE_DIR}/lib/chargenerator.cpp
  ${CIRCLE_DIR}/lib/classallocator.cpp
  ${CIRCLE_DIR}/lib/cputhrottle.cpp
  ${CIRCLE_DIR}/lib/debug.cpp
  ${CIRCLE_DIR}/lib/delayloop.S
  ${CIRCLE_DIR}/lib/device.cpp
  ${CIRCLE_DIR}/lib/devicenameservice.cpp
  ${CIRCLE_DIR}/lib/dmachannel.cpp
  ${CIRCLE_DIR}/lib/exceptionhandler.cpp
  ${CIRCLE_DIR}/lib/exceptionstub.S
  ${CIRCLE_DIR}/lib/gpioclock.cpp
  ${CIRCLE_DIR}/lib/gpiomanager.cpp
  ${CIRCLE_DIR}/lib/gpiopin.cpp
  ${CIRCLE_DIR}/lib/gpiopinfiq.cpp
  ${CIRCLE_DIR}/lib/heapallocator.cpp
  ${CIRCLE_DIR}/lib/i2cmaster.cpp
  ${CIRCLE_DIR}/lib/i2cslave.cpp
  ${CIRCLE_DIR}/lib/i2ssoundbasedevice.cpp
  ${CIRCLE_DIR}/lib/koptions.cpp
  ${CIRCLE_DIR}/lib/latencytester.cpp
  ${CIRCLE_DIR}/lib/logger.cpp
  ${CIRCLE_DIR}/lib/macaddress.cpp
  ${CIRCLE_DIR}/lib/machineinfo.cpp
  ${CIRCLE_DIR}/lib/memory.cpp
  ${CIRCLE_DIR}/lib/multicore.cpp
  ${CIRCLE_DIR}/lib/netdevice.cpp
  ${CIRCLE_DIR}/lib/new.cpp
  ${CIRCLE_DIR}/lib/nulldevice.cpp
  ${CIRCLE_DIR}/lib/numberpool.cpp
  ${CIRCLE_DIR}/lib/pageallocator.cpp
  ${CIRCLE_DIR}/lib/pagetable.cpp
  ${CIRCLE_DIR}/lib/ptrarray.cpp
  ${CIRCLE_DIR}/lib/ptrlist.cpp
  ${CIRCLE_DIR}/lib/purecall.cpp
  ${CIRCLE_DIR}/lib/pwmoutput.cpp
  ${CIRCLE_DIR}/lib/pwmsoundbasedevice.cpp
  ${CIRCLE_DIR}/lib/pwmsounddevice.cpp
  ${CIRCLE_DIR}/lib/qemu.cpp
  ${CIRCLE_DIR}/lib/screen.cpp
  ${CIRCLE_DIR}/lib/serial.cpp
  ${CIRCLE_DIR}/lib/setjmp.S
  ${CIRCLE_DIR}/lib/soundbasedevice.cpp
  ${CIRCLE_DIR}/lib/spimasteraux.cpp
  ${CIRCLE_DIR}/lib/spimaster.cpp
  ${CIRCLE_DIR}/lib/spimasterdma.cpp
  ${CIRCLE_DIR}/lib/spinlock.cpp
  ${CIRCLE_DIR}/lib/startup.S
  ${CIRCLE_DIR}/lib/string.cpp
  ${CIRCLE_DIR}/lib/synchronize.cpp
  ${CIRCLE_DIR}/lib/sysinit.cpp
  ${CIRCLE_DIR}/lib/time.cpp
  ${CIRCLE_DIR}/lib/timer.cpp
  ${CIRCLE_DIR}/lib/tracer.cpp
  ${CIRCLE_DIR}/lib/usertimer.cpp
  ${CIRCLE_DIR}/lib/util.cpp
  ${CIRCLE_DIR}/lib/util_fast.S
  ${CIRCLE_DIR}/lib/virtualgpiopin.cpp
  ${CIRCLE_DIR}/lib/writebuffer.cpp
  )

if (${RPI} EQUAL 4)
  target_sources(circle_lib PRIVATE
      ${CIRCLE_DIR}/lib/bcm54213.cpp
      ${CIRCLE_DIR}/lib/bcmpciehostbridge.cpp
      ${CIRCLE_DIR}/lib/bcmrandom200.cpp
      ${CIRCLE_DIR}/lib/interruptgic.cpp
      ${CIRCLE_DIR}/lib/dma4channel.cpp
      ${CIRCLE_DIR}/lib/devicetreeblob.cpp
      )
else()
  target_sources(circle_lib PRIVATE
    ${CIRCLE_DIR}/lib/bcmrandom.cpp
    ${CIRCLE_DIR}/lib/interrupt.cpp
    )
endif()


add_library(circle_linux STATIC)

target_sources(circle_linux PRIVATE
  ${CIRCLE_DIR}/addon/linux/timer.cpp
  ${CIRCLE_DIR}/addon/linux/interrupt.cpp
  ${CIRCLE_DIR}/addon/linux/linuxemu.cpp
  ${CIRCLE_DIR}/addon/linux/pthread.cpp
  ${CIRCLE_DIR}/addon/linux/linuxdevice.cpp
  ${CIRCLE_DIR}/addon/linux/platform_device.cpp
  ${CIRCLE_DIR}/addon/linux/device.cpp
  ${CIRCLE_DIR}/addon/linux/mutex.cpp
  ${CIRCLE_DIR}/addon/linux/raspberrypi-firmware.cpp
  ${CIRCLE_DIR}/addon/linux/semaphore.cpp
  ${CIRCLE_DIR}/addon/linux/kthread.cpp
  ${CIRCLE_DIR}/addon/linux/spinlock.cpp
  ${CIRCLE_DIR}/addon/linux/bug.cpp
  ${CIRCLE_DIR}/addon/linux/completion.cpp
  ${CIRCLE_DIR}/addon/linux/sprintf.cpp
  ${CIRCLE_DIR}/addon/linux/rwlock.cpp
  ${CIRCLE_DIR}/addon/linux/dma-mapping.cpp
  ${CIRCLE_DIR}/addon/linux/printk.cpp
  ${CIRCLE_DIR}/addon/linux/delay.cpp
  )

add_library(circle_sdcard STATIC)

target_sources(circle_sdcard PRIVATE
  ${CIRCLE_DIR}/addon/SDCard/emmc.cpp
  ${CIRCLE_DIR}/addon/SDCard/mmchost.cpp
  ${CIRCLE_DIR}/addon/SDCard/sample/kernel.cpp
  ${CIRCLE_DIR}/addon/SDCard/sample/main.cpp
  ${CIRCLE_DIR}/addon/SDCard/sdhost.cpp
  )

add_library(circle_vchiq STATIC)

target_sources(circle_vchiq PRIVATE
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_kern_lib.c
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_shim.c
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_connected.c
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_2835_arm.c
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_core.c
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_arm.c
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiqdevice.cpp
  ${CIRCLE_DIR}/addon/vc4/vchiq/vchiq_util.c
  )

add_library(circle_sound STATIC)

target_sources(circle_sound PRIVATE
  ${CIRCLE_DIR}/addon/vc4/sound/vchiqsoundbasedevice.cpp
  ${CIRCLE_DIR}/addon/vc4/sound/vchiqsounddevice.cpp
  )

add_library(circle_usb STATIC)

target_sources(circle_usb PRIVATE
  ${CIRCLE_DIR}/lib/usb/usbgamepadps4.cpp
  ${CIRCLE_DIR}/lib/usb/usbkeyboard.cpp
  ${CIRCLE_DIR}/lib/usb/usbrequest.cpp
  ${CIRCLE_DIR}/lib/usb/usbdevicefactory.cpp
  ${CIRCLE_DIR}/lib/usb/usbstring.cpp
  ${CIRCLE_DIR}/lib/usb/usbserialcdc.cpp
  ${CIRCLE_DIR}/lib/usb/usbgamepad.cpp
  ${CIRCLE_DIR}/lib/usb/usbmassdevice.cpp
  ${CIRCLE_DIR}/lib/usb/usbgamepadstandard.cpp
  ${CIRCLE_DIR}/lib/usb/usbdevice.cpp
  ${CIRCLE_DIR}/lib/usb/lan7800.cpp
  ${CIRCLE_DIR}/lib/usb/usbserialpl2303.cpp
  ${CIRCLE_DIR}/lib/usb/usbserialch341.cpp
  ${CIRCLE_DIR}/lib/usb/usbmidi.cpp
  ${CIRCLE_DIR}/lib/usb/usbprinter.cpp
  ${CIRCLE_DIR}/lib/usb/usbserial.cpp
  ${CIRCLE_DIR}/lib/usb/usbfunction.cpp
  ${CIRCLE_DIR}/lib/usb/usbstandardhub.cpp
  ${CIRCLE_DIR}/lib/usb/usbconfigparser.cpp
  ${CIRCLE_DIR}/lib/usb/usbgamepadxbox360.cpp
  ${CIRCLE_DIR}/lib/usb/usbmouse.cpp
  ${CIRCLE_DIR}/lib/usb/usbhostcontroller.cpp
  ${CIRCLE_DIR}/lib/usb/usbhiddevice.cpp
  ${CIRCLE_DIR}/lib/usb/usbgamepadps3.cpp
  ${CIRCLE_DIR}/lib/usb/usbgamepadswitchpro.cpp
  ${CIRCLE_DIR}/lib/usb/usbbluetooth.cpp
  ${CIRCLE_DIR}/lib/usb/usbgamepadxboxone.cpp
  ${CIRCLE_DIR}/lib/usb/usbserialft231x.cpp
  ${CIRCLE_DIR}/lib/usb/usbendpoint.cpp
  ${CIRCLE_DIR}/lib/usb/smsc951x.cpp
  ${CIRCLE_DIR}/lib/usb/usbcdcethernet.cpp
  ${CIRCLE_DIR}/lib/usb/usbserialcp2102.cpp
  )

if (${RPI} EQUAL 4)
  target_sources(circle_usb PRIVATE
    ${CIRCLE_DIR}/lib/usb/xhciring.cpp
    ${CIRCLE_DIR}/lib/usb/xhciroothub.cpp
    ${CIRCLE_DIR}/lib/usb/xhcidevice.cpp
    ${CIRCLE_DIR}/lib/usb/xhciusbdevice.cpp
    ${CIRCLE_DIR}/lib/usb/xhcislotmanager.cpp
    ${CIRCLE_DIR}/lib/usb/xhcimmiospace.cpp
    ${CIRCLE_DIR}/lib/usb/xhcicommandmanager.cpp
    ${CIRCLE_DIR}/lib/usb/xhciendpoint.cpp
    ${CIRCLE_DIR}/lib/usb/xhcirootport.cpp
    ${CIRCLE_DIR}/lib/usb/xhcisharedmemallocator.cpp
    ${CIRCLE_DIR}/lib/usb/xhcieventmanager.cpp
    )
else()
  target_sources(circle_usb PRIVATE
    ${CIRCLE_DIR}/lib/usb/dwhcixactqueue.cpp
    ${CIRCLE_DIR}/lib/usb/dwhcirootport.cpp
    ${CIRCLE_DIR}/lib/usb/dwhcidevice.cpp
    ${CIRCLE_DIR}/lib/usb/dwhciframeschednper.cpp
    ${CIRCLE_DIR}/lib/usb/dwhciregister.cpp
    ${CIRCLE_DIR}/lib/usb/dwhciframeschedper.cpp
    ${CIRCLE_DIR}/lib/usb/dwhcixferstagedata.cpp
    ${CIRCLE_DIR}/lib/usb/dwhciframeschednsplit.cpp
    )
endif()

add_library(circle_sched STATIC)

target_sources(circle_sched PRIVATE
  ${CIRCLE_DIR}/lib/sched/task.cpp
  ${CIRCLE_DIR}/lib/sched/scheduler.cpp
  ${CIRCLE_DIR}/lib/sched/taskswitch.S
  ${CIRCLE_DIR}/lib/sched/synchronizationevent.cpp
  )

add_library(circle_input STATIC)

target_sources(circle_input PRIVATE
  ${CIRCLE_DIR}/lib/input/console.cpp
  ${CIRCLE_DIR}/lib/input/keyboardbehaviour.cpp
  ${CIRCLE_DIR}/lib/input/keyboardbuffer.cpp
  ${CIRCLE_DIR}/lib/input/keymap.cpp
  ${CIRCLE_DIR}/lib/input/linediscipline.cpp
  ${CIRCLE_DIR}/lib/input/mousebehaviour.cpp
  ${CIRCLE_DIR}/lib/input/mouse.cpp
  ${CIRCLE_DIR}/lib/input/touchscreen.cpp
  )

add_library(circle_fs)

target_sources(circle_fs PRIVATE
  ${CIRCLE_DIR}/lib/fs/partition.cpp
  ${CIRCLE_DIR}/lib/fs/partitionmanager.cpp
  )

add_library(memu_cir INTERFACE)

target_include_directories(memu_cir INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  )

target_sources(memu_cir INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/diskio_cir.cpp
  ${CMAKE_CURRENT_LIST_DIR}/kernel.cpp
  ${CMAKE_CURRENT_LIST_DIR}/keyeventbuffer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/main_cir.cpp
  )
