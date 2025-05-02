#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from pylab import rcParams

# times = [int(x) for x in times_data.strip('[] ').split(' ')]
filenames = [ ['x_float_graveyard_soa','Graveyard SoA'],
              ['x_float_graveyard_aos','Graveyard AoS'],
              ['x_float_ordered_soa','Ordered SoA'],
              ['x_float_ordered_aos','Ordered AoS'],
              ['x_float_linear_soa','Linear SoA'],
              ['x_float_linear_aos','Linear AoS'],
            ]

fig,ax = plt.subplots(1,2)
df = []

for i in range(len(filenames)):
    df.append(pd.read_csv(filenames[i][0], header=1, skipinitialspace=True))
    df[i] = df[i].assign(
            **{'ops_per_sec': lambda x:x['ops'] / x['mean']}
            )

    ax[0].scatter(df[i]['x'].values, df[i]['mean'].values,
            label=filenames[i][1], s=rcParams['lines.markersize']*2)
    ax[1].plot(df[i]['x'].values, df[i]['ops_per_sec'].values,
            label=filenames[i][1])

ax[0].set(xlim=(0,200))
ax[0].legend(loc='upper left')
ax[0].set_title('Mean time for 1M operations vs load factor')
ax[0].set_xlabel('x')
ax[0].set_ylabel('time (s)')

ax[1].set(xlim=(0,200))
ax[1].legend(loc='upper right')
ax[1].set_title('Operations/sec vs load factor')
ax[1].set_xlabel('x')
ax[1].set_ylabel('millions of operations / sec')
ax[1].yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6))
        )

plt.legend()
plt.show()
