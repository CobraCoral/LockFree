#!/usr/bin/env python
import os
for i in range(10000,2010000,10000):
    a=os.popen('./st.exe %d'%i)
    alines = a.readlines();
    b=os.popen('./lf.exe %d'%i)
    blines = b.readlines();
    c=os.popen('./mutex.exe %d'%i)
    clines = c.readlines();
    print i, alines[0][:-1], blines[0][:-1], clines[0][:-1]
