#!/usr/bin/env python3
"""RF Learn: declarative config-proxy writing into RF Send's code block.

RF Learn is the 'editor' side of this companion pair (same role APRS Edit
plays for APRS TX): it carries this descriptor as its own asset prefix,
then calls the ordinary cfg_save() in its C code -- the resident loader
redirects that write into RF Send's asset area in place. RF Send then
reads its own (now overwritten) asset area via asset_read()."""
import os, struct, sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
from app_assets import Assets, cdefine

PROXY_MAGIC = cdefine("app_config_proxy.h", "APP_CONFIG_PROXY_MAGIC")
CODE_MAGIC = cdefine("rf_code.h", "RF_CODE_MAGIC")
CODE_LEN = cdefine("rf_code.h", "RF_CODE_LEN")
TARGET = b"RF Send"

proxy = struct.pack("<I16sHBBI", PROXY_MAGIC, TARGET.ljust(16, b"\0"),
                    0, CODE_LEN, 0, CODE_MAGIC)
if len(proxy) != 28:
    raise ValueError("configuration proxy descriptor layout changed")

a = Assets("RFLEARN")
a.prefix("CFG_PROXY", proxy)
a.main()
