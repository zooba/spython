"""
Test file containing a string that Windows Defender will identify as a virus.

To verify your build, launch 'spython amsi_test.py'.
"""

def get_amsi_test_string():
    import base64
    # The string is encrypted to avoid detection on disk.
    return bytes(
        x ^ 0x33 for x in
        base64.b64decode(b"FHJ+YHoTZ1ZARxNgUl5DX1YJEwRWBAFQAFBWHgsFAlEeBwAACh4LBAcDHgNSUAIHCwdQAgALBRQ=")
    ).decode()

if __name__ == "__main__":
    print(eval(get_amsi_test_string()))
