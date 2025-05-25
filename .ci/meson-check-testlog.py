#!/usr/bin/env python3

import sys
import re
from pathlib import Path

log = Path(sys.argv[1]).read_text()
ok = int(re.search('Ok:[\s]*([0-9]+)', log).group(1))
fail = int(re.search('Fail:[\s]*([0-9]+)', log).group(1))
if ok == 0 or fail > 0:
    print(log)
    exit(1)
