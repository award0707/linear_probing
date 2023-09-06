#!/usr/bin/env python3

import sys
import pandas as pd
import glob
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from pylab import rcParams

"""
def smooth(scalars: list[float], weight: float) -> list[float]:
    last = scalars[0]
    smoothed = list()
    for point in scalars:
        smoothed_val = last * weight + (1 - weight) * point
        smoothed.append(smoothed_val)
        last = smoothed_val          # Anchor the last smoothed value

    return smoothed
"""

if len(sys.argv) < 2:
    fn = glob.glob('loadbench*')
    print(fn)
else:
    fn = sys.argv[1:]
    print(fn)

fig,ax = plt.subplots()
df = []
dotsize = rcParams['lines.markersize']**2
capends = lambda s: (s[:1].upper() + s[1:-1] + s[-1:].upper())[:len(s)]

for i in range(len(fn)):
    splitfn = fn[i].split('_')[1:]
    legend = ' '.join([splitfn[0].title(), capends(splitfn[1]),splitfn[2]+'M'])
    print(fn[i], legend)
    df.append(pd.read_csv(fn[i], header=2, skipinitialspace=True,
        skipfooter=2, delim_whitespace=True,engine='python'))

    ax.plot(df[i]['Operations'].values, df[i]['Ops_per_sec'].values,
            label=legend)  # s = dotsize

ax.legend(loc='upper right')
ax.set_title('Operations per second')
ax.set_xlabel('millions of operations')
ax.set_ylabel('millions of operations / sec')
ax.yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6)))
ax.xaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6)))

plt.show()

