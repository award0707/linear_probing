#!/usr/bin/env python3

import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from pylab import rcParams

if len(sys.argv) < 2:
    print('provide at least 1 file to plot')
    exit()
else:
    fn = sys.argv[1:]
    print(fn)

fig,ax = plt.subplots()
df = []

for i in range(len(fn)):
    df.append(pd.read_csv(fn[i], header=1, skipinitialspace=True))
    df[i] = df[i].assign(
            **{'Mean queries/sec': lambda x:x['Queries/trial'] / x['Mean']}
            )

    ax.scatter(df[i]['x'].values, df[i]['Mean'].values,
            label=fn[i], s=rcParams['lines.markersize']*2)
#    ax[1].plot(df[i]['x'].values, df[i]['Mean queries/sec'].values,
#            label=fn[i])

ax.set(xlim=(0,500))
ax.legend(loc='upper left')
ax.set_title('Mean time for 1M queries vs load factor')
ax.set_xlabel('x')
ax.set_ylabel('time (s)')
"""
ax[1].set(xlim=(0,500))
ax[1].legend(loc='upper right')
ax[1].set_title('Queries per second vs load factor')
ax[1].set_xlabel('x')
ax[1].set_ylabel('millions of queries / sec')
ax[1].yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6))
        )
"""
plt.legend()
plt.show()
