#!/usr/bin/env python3
"""Receive FirmDump's USB stream and save each region as a file.

    scripts/firmdump-recv.py [-o DIR]

Start this first, then press F2 in FirmDump and plug the cable in. Every
region the calculator sends is saved as DIR/<name>.bin (OS.bin, ROM.bin or
the custom base, e.g. 80400000.bin); a transfer cut short is kept as
<name>.bin.partial. It keeps waiting for more until Ctrl+C.

The calculator speaks fxlink's protocol (gint's usb_ff_bulk interface), so
`fxlink -i` works too. This script only needs the libusb-1.0 shared
library (present on most Linux systems; `brew install libusb` on macOS),
used through ctypes: no Python packages, no headers.

Linux: reading the device needs permission. Once, as root:
    echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="07cf", ATTRS{idProduct}=="6102", MODE="0660", GROUP="plugdev"' \\
      | sudo tee /etc/udev/rules.d/50-casio-fxlink.rules
    sudo udevadm control --reload
then plug the calculator in again (you must be in the plugdev group).
"""

import argparse
import ctypes
import ctypes.util
import os
import struct
import sys
import time

VENDOR, PRODUCT = 0x07CF, 0x6102        # gint's USB driver on the fx-CG
FXLINK_CLASS, FXLINK_SUBCLASS = 0xFF, 0x77
HEADER = struct.Struct("<III16s16s")    # version, size, transfer size, app, type

ERROR_TIMEOUT, ERROR_NO_DEVICE, ERROR_ACCESS = -7, -4, -3

UDEV_HINT = """\
Permission denied opening the calculator. Allow it once (as root):
  echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="07cf", ATTRS{idProduct}=="6102", \
MODE="0660", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/50-casio-fxlink.rules
  sudo udevadm control --reload
then unplug and plug the calculator in again."""


# libusb structures (libusb.h)

class DeviceDescriptor(ctypes.Structure):
    _fields_ = [("bLength", ctypes.c_uint8), ("bDescriptorType", ctypes.c_uint8),
                ("bcdUSB", ctypes.c_uint16), ("bDeviceClass", ctypes.c_uint8),
                ("bDeviceSubClass", ctypes.c_uint8), ("bDeviceProtocol", ctypes.c_uint8),
                ("bMaxPacketSize0", ctypes.c_uint8), ("idVendor", ctypes.c_uint16),
                ("idProduct", ctypes.c_uint16), ("bcdDevice", ctypes.c_uint16),
                ("iManufacturer", ctypes.c_uint8), ("iProduct", ctypes.c_uint8),
                ("iSerialNumber", ctypes.c_uint8), ("bNumConfigurations", ctypes.c_uint8)]


class EndpointDescriptor(ctypes.Structure):
    _fields_ = [("bLength", ctypes.c_uint8), ("bDescriptorType", ctypes.c_uint8),
                ("bEndpointAddress", ctypes.c_uint8), ("bmAttributes", ctypes.c_uint8),
                ("wMaxPacketSize", ctypes.c_uint16), ("bInterval", ctypes.c_uint8),
                ("bRefresh", ctypes.c_uint8), ("bSynchAddress", ctypes.c_uint8),
                ("extra", ctypes.c_void_p), ("extra_length", ctypes.c_int)]


class InterfaceDescriptor(ctypes.Structure):
    _fields_ = [("bLength", ctypes.c_uint8), ("bDescriptorType", ctypes.c_uint8),
                ("bInterfaceNumber", ctypes.c_uint8), ("bAlternateSetting", ctypes.c_uint8),
                ("bNumEndpoints", ctypes.c_uint8), ("bInterfaceClass", ctypes.c_uint8),
                ("bInterfaceSubClass", ctypes.c_uint8), ("bInterfaceProtocol", ctypes.c_uint8),
                ("iInterface", ctypes.c_uint8),
                ("endpoint", ctypes.POINTER(EndpointDescriptor)),
                ("extra", ctypes.c_void_p), ("extra_length", ctypes.c_int)]


class Interface(ctypes.Structure):
    _fields_ = [("altsetting", ctypes.POINTER(InterfaceDescriptor)),
                ("num_altsetting", ctypes.c_int)]


class ConfigDescriptor(ctypes.Structure):
    _fields_ = [("bLength", ctypes.c_uint8), ("bDescriptorType", ctypes.c_uint8),
                ("wTotalLength", ctypes.c_uint16), ("bNumInterfaces", ctypes.c_uint8),
                ("bConfigurationValue", ctypes.c_uint8), ("iConfiguration", ctypes.c_uint8),
                ("bmAttributes", ctypes.c_uint8), ("MaxPower", ctypes.c_uint8),
                ("interface", ctypes.POINTER(Interface)),
                ("extra", ctypes.c_void_p), ("extra_length", ctypes.c_int)]


def load_libusb():
    names = [ctypes.util.find_library("usb-1.0"), "libusb-1.0.so.0",
             "libusb-1.0.dylib", "/opt/homebrew/lib/libusb-1.0.dylib",
             "/usr/local/lib/libusb-1.0.dylib"]
    for name in filter(None, names):
        try:
            lib = ctypes.CDLL(name)
            break
        except OSError:
            continue
    else:
        sys.exit("libusb-1.0 not found (Linux: install libusb-1.0-0; macOS: brew install libusb)")

    P = ctypes.c_void_p
    lib.libusb_init.argtypes = [ctypes.POINTER(P)]
    lib.libusb_get_device_list.argtypes = [P, ctypes.POINTER(ctypes.POINTER(P))]
    lib.libusb_get_device_list.restype = ctypes.c_ssize_t
    lib.libusb_free_device_list.argtypes = [ctypes.POINTER(P), ctypes.c_int]
    lib.libusb_get_device_descriptor.argtypes = [P, ctypes.POINTER(DeviceDescriptor)]
    lib.libusb_get_active_config_descriptor.argtypes = [
        P, ctypes.POINTER(ctypes.POINTER(ConfigDescriptor))]
    lib.libusb_free_config_descriptor.argtypes = [ctypes.POINTER(ConfigDescriptor)]
    lib.libusb_open.argtypes = [P, ctypes.POINTER(P)]
    lib.libusb_close.argtypes = [P]
    lib.libusb_set_auto_detach_kernel_driver.argtypes = [P, ctypes.c_int]
    lib.libusb_claim_interface.argtypes = [P, ctypes.c_int]
    lib.libusb_release_interface.argtypes = [P, ctypes.c_int]
    lib.libusb_bulk_transfer.argtypes = [P, ctypes.c_ubyte, ctypes.c_void_p, ctypes.c_int,
                                         ctypes.POINTER(ctypes.c_int), ctypes.c_uint]
    lib.libusb_error_name.restype = ctypes.c_char_p
    return lib


class Calculator:
    """An open calculator: its fxlink interface claimed, its bulk IN endpoint"""

    def __init__(self, lib, handle, intf, ep_in):
        self.lib, self.handle, self.intf, self.ep_in = lib, handle, intf, ep_in

    def read(self, buf, timeout_ms):
        """Bytes read (maybe empty on a timeout); None once unplugged"""
        n = ctypes.c_int(0)
        rc = self.lib.libusb_bulk_transfer(self.handle, self.ep_in, buf, len(buf),
                                           ctypes.byref(n), timeout_ms)
        if rc == ERROR_NO_DEVICE:
            return None
        if rc not in (0, ERROR_TIMEOUT):
            raise OSError(f"bulk transfer failed: {self.lib.libusb_error_name(rc).decode()}")
        return ctypes.string_at(buf, n.value)

    def close(self):
        self.lib.libusb_release_interface(self.handle, self.intf)
        self.lib.libusb_close(self.handle)


def fxlink_interface(lib, dev):
    """(interface number, bulk IN endpoint) of gint's fxlink interface, or None"""
    cfg = ctypes.POINTER(ConfigDescriptor)()
    if lib.libusb_get_active_config_descriptor(dev, ctypes.byref(cfg)) != 0:
        return None
    try:
        for i in range(cfg.contents.bNumInterfaces):
            itf = cfg.contents.interface[i]
            for a in range(itf.num_altsetting):
                alt = itf.altsetting[a]
                if (alt.bInterfaceClass, alt.bInterfaceSubClass) != (FXLINK_CLASS, FXLINK_SUBCLASS):
                    continue
                for e in range(alt.bNumEndpoints):
                    ep = alt.endpoint[e]
                    if ep.bEndpointAddress & 0x80 and ep.bmAttributes & 3 == 2:
                        return alt.bInterfaceNumber, ep.bEndpointAddress
    finally:
        lib.libusb_free_config_descriptor(cfg)
    return None


def find_calculator(lib, ctx):
    """The calculator once it is plugged in and gint set USB up, or None"""
    devs = ctypes.POINTER(ctypes.c_void_p)()
    count = lib.libusb_get_device_list(ctx, ctypes.byref(devs))
    try:
        for i in range(max(count, 0)):
            desc = DeviceDescriptor()
            lib.libusb_get_device_descriptor(devs[i], ctypes.byref(desc))
            if (desc.idVendor, desc.idProduct) != (VENDOR, PRODUCT):
                continue
            handle = ctypes.c_void_p()
            rc = lib.libusb_open(devs[i], ctypes.byref(handle))
            if rc == ERROR_ACCESS:
                sys.exit(UDEV_HINT)
            if rc != 0:
                continue
            found = fxlink_interface(lib, devs[i])
            if not found:
                lib.libusb_close(handle)
                continue
            intf, ep_in = found
            lib.libusb_set_auto_detach_kernel_driver(handle, 1)
            if lib.libusb_claim_interface(handle, intf) != 0:
                lib.libusb_close(handle)
                continue
            return Calculator(lib, handle, intf, ep_in)
    finally:
        if count >= 0:
            lib.libusb_free_device_list(devs, 1)
    return None


class Receiver:
    """Splits the byte stream into fxlink messages; firmdump's go to files"""

    def __init__(self, out_dir):
        self.out_dir = out_dir
        self.pending = b""          # bytes of a header not complete yet
        self.msg = None             # (app, type, size) being received
        self.got = 0
        self.file = None
        self.path = None
        self.start = 0.0

    def feed(self, data):
        while data:
            if self.msg is None:
                self.pending += data
                if len(self.pending) < HEADER.size:
                    return
                head, data = self.pending[:HEADER.size], self.pending[HEADER.size:]
                self.pending = b""
                self.begin(*HEADER.unpack(head))
                continue
            take = min(len(data), self.msg[2] - self.got)
            self.chunk(data[:take])
            data = data[take:]
            if self.got == self.msg[2]:
                self.finish()

    def begin(self, version, size, _transfer, app, mtype):
        app = app.rstrip(b"\0").decode("ascii", "replace")
        mtype = mtype.rstrip(b"\0").decode("ascii", "replace")
        if version != 0x100:
            print(f"warning: unknown fxlink version 0x{version:x}", file=sys.stderr)
        self.msg, self.got, self.start = (app, mtype, size), 0, time.monotonic()
        self.text = b""
        if app == "firmdump":
            self.path = self.free_name(mtype or "dump")
            self.file = open(self.path + ".partial", "wb")
            print(f"receiving {mtype} ({size >> 10} KB) -> {self.path}")
        if size == 0:
            self.finish()

    def chunk(self, data):
        self.got += len(data)
        if self.file:
            self.file.write(data)
            secs = max(time.monotonic() - self.start, 1e-6)
            print(f"\r  {self.got >> 10} / {self.msg[2] >> 10} KB  "
                  f"({self.got / secs / 1024:.0f} KB/s)", end="", flush=True)
        elif self.msg[:2] == ("fxlink", "text"):
            self.text += data

    def finish(self):
        app, mtype, size = self.msg
        if self.file:
            self.file.close()
            os.replace(self.path + ".partial", self.path)
            print(f"\n  saved {self.path} ({size} bytes)")
        elif (app, mtype) == ("fxlink", "text"):
            sys.stdout.write(self.text.decode("utf-8", "replace"))
        else:
            print(f"ignored a message {app}/{mtype} of {size} bytes")
        self.msg, self.file = None, None

    def disconnected(self):
        """The link dropped: keep what arrived of a cut message as .partial"""
        if self.file:
            self.file.close()
            print(f"\n  cut short at {self.got} of {self.msg[2]} bytes: "
                  f"kept as {self.path}.partial")
        self.msg, self.file, self.pending = None, None, b""

    def free_name(self, name):
        path = os.path.join(self.out_dir, name + ".bin")
        n = 1
        while os.path.exists(path) or os.path.exists(path + ".partial"):
            path = os.path.join(self.out_dir, f"{name}-{n}.bin")
            n += 1
        return path


def main():
    ap = argparse.ArgumentParser(description="Save FirmDump's USB stream to files.")
    ap.add_argument("-o", "--out", default=".", help="folder for the .bin files (default: here)")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    lib = load_libusb()
    ctx = ctypes.c_void_p()
    if lib.libusb_init(ctypes.byref(ctx)) != 0:
        sys.exit("libusb_init failed")

    rx = Receiver(args.out)
    buf = (ctypes.c_ubyte * 16384)()
    try:
        while True:
            print("waiting for the calculator (F2 in FirmDump, then plug the cable in)...")
            calc = None
            while calc is None:
                calc = find_calculator(lib, ctx)
                if calc is None:
                    time.sleep(0.5)
            print("connected")
            while True:
                data = calc.read(buf, 500)
                if data is None:
                    rx.disconnected()
                    print("calculator disconnected")
                    break
                rx.feed(data)
            calc.close()
    except KeyboardInterrupt:
        rx.disconnected()
        print()


if __name__ == "__main__":
    main()
