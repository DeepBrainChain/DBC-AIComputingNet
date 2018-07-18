#!/usr/bin/env python

import mt
import re
import sys


def parse(f):
    m = re.compile(r'^.+(decode buf:|send buf:)([\s\da-fA-F]+)')

    try:
        while True:
            s = f.readline()

            if not s:  # EOF
                exit(0)

            s = s.strip()
            t=m.match(s)

            if t:
                print s
                mt.decode(t.group(2))
                print
            else:
                print s
    except KeyboardInterrupt:
        exit(1)

if __name__=="__main__":
    if len(sys.argv) == 2:
        filename = sys.argv[1]
        # filename = "./matrix_core_0.log"
        with open(filename) as f:
            parse(f)

    else:
        parse(sys.stdin)

