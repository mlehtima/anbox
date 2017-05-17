#!/bin/sh

ANBOX_DATA=/var/lib/anbox

OVERLAY_DIR=$ANBOX_DATA/overlay
ROOTFS=$ANBOX_DATA/rootfs
ROOTFS_FILE=$ANBOX_DATA/android.img

EXTRA_BIND_MOUNTS=/var/lib/anbox/containers/default/extra_bind_mounts

LIBS="/vendor/lib/egl/libEGL_adreno.so /vendor/lib/egl/libGLESv1_CM_adreno.so /vendor/lib/egl/libGLESv2_adreno.so /system/lib/hw/gralloc.$(getprop ro.product.device).so"

# we have no ldd for android libs
find_deps() {
    if [ "$2" != "" ]; then
        eval "PREV_DEPS=\$$1"
        if [[ "$PREV_DEPS" != *"$2"* ]]; then
            eval "$1+='$2 '"
            A=$(find /vendor/ /system/ | grep $2)
            if [ "$A" != "" ]; then
                D=$(strings $A | grep "\.so$" | grep -v $2)
                for f in $D; do
                    find_deps $1 $f
                done
            fi
        fi
    fi
}

echo "Finding dependancies of $LIBS"
DEPS=
for f in $LIBS; do
    find_deps DEPS $f
done

echo "Found dependancies"
echo $DEPS

echo "Ignoring files which are already in rootfs"
mount $ROOTFS_FILE $ROOTFS

NEW_DEPS=
for f in $DEPS; do
    if [ "$(find $ROOTFS/system/ | grep $f)" == "" ]; then
        NEW_DEPS+="$f "
    else
        echo $f" already in rootfs"
    fi
done

umount $ROOTFS

DEPS=$NEW_DEPS

echo "Copying everything to overlay"
for f in $DEPS; do
    # don't put find /vendor/ /system/. because we need to preffer the /vendor location
    FULL_NAME=$(find /vendor/ | grep $(basename $f))
    if [ "$FULL_NAME" == "" ]; then
        FULL_NAME=$(find /system/ | grep $(basename $f))
    fi
    if [ "$FULL_NAME" != "" ]; then
        echo $f" -> "$FULL_NAME
        mkdir -p $(dirname $OVERLAY_DIR/$FULL_NAME)
        cp $FULL_NAME $OVERLAY_DIR/$FULL_NAME
    else
        echo "ignoring possibly optional depandancy: $f"
    fi
done

mkdir -p $OVERLAY_DIR/vendor/lib/egl
mkdir -p $OVERLAY_DIR/system/lib/egl
mkdir -p $OVERLAY_DIR/system/lib/hw
cp /system/lib/egl/egl.cfg $OVERLAY_DIR/system/lib/egl/egl.cfg
cp /system/lib/hw/gralloc.$(getprop ro.product.device).so $OVERLAY_DIR/system/lib/hw/gralloc.default.so
cp /system/lib/hw/gralloc.$(getprop ro.product.device).so $OVERLAY_DIR/system/lib/hw/gralloc.goldfish.so

cd $OVERLAY_DIR
mkdir -p vendor/lib/egl
ln -s /vendor/lib/egl/libGLESv2_adreno.so system/lib/egl/libGLESv2_emulation.so   
ln -s /vendor/lib/egl/libGLESv1_CM_adreno.so system/lib/egl/libGLESv1_CM_emulation.so
ln -s /vendor/lib/egl/libEGL_adreno.so system/lib/egl/libEGL_emulation.so      

echo "Setting up extra bind mounts"
echo "/dev/ion"
echo "/dev/ion dev/ion" > $EXTRA_BIND_MOUNTS

for f in /dev/kgsl*; do
    echo $f
    echo "/dev/$(basename $f) dev/$(basename $f)" >> $EXTRA_BIND_MOUNTS
done

echo "Setting up wayland socket"

echo "/run/display"
echo "/run/display run/display" >> $EXTRA_BIND_MOUNTS

