from pathlib import Path
from shutil import which
from textwrap import dedent

EMCC_PATH = Path(which("emcc") + ".py")

emcc_text = EMCC_PATH.read_text()
emcc_lines = emcc_text.splitlines()
emcc_lines.insert(
    1,
    dedent(
        """
        import logging
        for name in ["cache", "system_libs", "shared"]:
            logger = logging.getLogger(name)
            logger.setLevel(logging.WARN)
        """
    ),
)

EMCC_PATH.write_text("\n".join(emcc_lines))
