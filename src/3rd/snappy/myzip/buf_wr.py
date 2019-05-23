

import sys

if len(sys.argv) <3 : 
    print "python " + sys.argv[0] + " [filename] [string]"
    exit(0)

fn = sys.argv[1]
s = sys.argv[2]
s = s.split()
c = [int(i,16) for i in s]

f = open(fn, "wb")

f.write(''.join(chr(i) for i in c))

f.close()
