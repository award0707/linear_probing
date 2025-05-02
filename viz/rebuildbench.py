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
    fn = glob.glob('*_rebuilds')
    print(fn)
else:
    fn = sys.argv[1:]
    print(fn)

fig,ax = plt.subplots()
df = []
dotsize = rcParams['lines.markersize']**2

for i in range(len(fn)):
    splitfn = fn[i].split('_')
    splitfn[0] += 'K'
    legend = ' '.join(splitfn).title()
    print(fn[i], legend)
    df.append(pd.read_csv(fn[i], header=1, skipinitialspace=True,
        engine='python'))

    ax.plot(df[i]['x'].values, df[i]['Mean'].values,
            label=legend)  # s = dotsize

ax.legend(loc='upper right')
ax.set_title('Rebuild time vs load factor')
ax.set_xlabel('x')
ax.set_ylabel('mean time (s)')
plt.xlim(2,1000)
ax.yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6)))

plt.show()

