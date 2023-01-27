"""
Prevent emcc from printing info level logging

dejagnu will fail tests because of these log statements and the messages depend
on emscripten cache state so xfailing the right tests is very hard.

See emscripten issue:
https://github.com/emscripten-core/emscripten/issues/18607
"""

from pathlib import Path
from shutil import which
from sys import exit
from textwrap import dedent

EMCC_PATH = Path(which("emcc") + ".py")

emcc_text = EMCC_PATH.read_text()
if "# quiet_emcc_info patch" in emcc_text:
    exit(0)

emcc_lines = emcc_text.splitlines()
emcc_lines.insert(
    1,
    dedent(
        """
        # quiet_emcc_info patch
        import logging
        for name in ["cache", "system_libs", "shared"]:
            logger = logging.getLogger(name)
            logger.setLevel(logging.WARN)
        """
    ),
)

EMCC_PATH.write_text("\n".join(emcc_lines))
