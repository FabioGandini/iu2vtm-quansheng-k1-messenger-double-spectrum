#!/usr/bin/env python3
"""RF Send: placeholder code block (magic + zeros), overwritten in place by
RF Learn (the proxy/editor side) via its redirected cfg_save()."""
import os, sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
from app_assets import Assets, cdefine

CODE_MAGIC = cdefine("rf_code.h", "RF_CODE_MAGIC")
CODE_LEN = cdefine("rf_code.h", "RF_CODE_LEN")

placeholder = CODE_MAGIC.to_bytes(4, "little") + b"\x00" * (CODE_LEN - 4)
if len(placeholder) != CODE_LEN:
    raise ValueError("rf_code.h layout changed")

a = Assets("RFSEND")
a.prefix("CODE_BLOCK", placeholder)
a.main()
