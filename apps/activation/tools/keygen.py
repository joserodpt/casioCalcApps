#!/usr/bin/env python3
# Activation codes for the Activation add-in: give it the 6-character
# request code the calculator shows and it prints the activation code to
# type in. The hashes and salts must match src/main.c.
#   apps/activation/tools/keygen.py 3FA91C
#   apps/activation/tools/keygen.py --id 0123456789ABCDEF   from a product ID
import sys

SALT_REQUEST = b"casioCalcApps/activation/request"
SALT_ACTIVATE = b"casioCalcApps/activation/activate"


def code(salt, data):
    """FNV-1a over salt + data, MurmurHash3's finaliser, top 24 bits."""
    h = 2166136261
    for b in salt + data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    h ^= h >> 16
    h = (h * 0x85EBCA6B) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0xC2B2AE35) & 0xFFFFFFFF
    h ^= h >> 16
    return f"{h >> 8:06X}"


def activation(request):
    return code(SALT_ACTIVATE, request.encode())


def main(args):
    if len(args) == 2 and args[0] == "--id":
        hex_id = args[1].replace(" ", "")
        if len(hex_id) != 16:
            sys.exit("the product ID is 16 hex digits")
        request = code(SALT_REQUEST, bytes.fromhex(hex_id))
        print(f"request code:    {request}")
        print(f"activation code: {activation(request)}")
        return
    if len(args) != 1:
        sys.exit(__doc__ or "usage: keygen.py <request code> | --id <product ID>")
    request = args[0].strip().upper()
    if len(request) != 6 or any(c not in "0123456789ABCDEF" for c in request):
        sys.exit("the request code is 6 characters, 0-9 and A-F")
    print(activation(request))


if __name__ == "__main__":
    main(sys.argv[1:])
