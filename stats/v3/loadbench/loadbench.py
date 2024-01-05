#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from pylab import rcParams

"""
def smooth(scalars: list[float], weight: float) -> list[float]:  # Weight between 0 and 1
    last = scalars[0]  # First value in the plot (first timestep)
    smoothed = list()
    for point in scalars:
        smoothed_val = last * weight + (1 - weight) * point  # Calculate smoothed value
        smoothed.append(smoothed_val)                        # Save it
        last = smoothed_val                                  # Anchor the last smoothed value
        
    return smoothed
"""

fn = [ ['loadbench_graveyard_soa','Graveyard SoA'],
       ['loadbench_graveyard_aos','Graveyard AoS'],
       ['loadbench_ordered_soa','Ordered SoA'],
       ['loadbench_ordered_aos','Ordered AoS'],
       ['loadbench_linear_soa','Linear SoA'],
       ['loadbench_linear_aos','Linear AoS'] ]

fig,ax = plt.subplots()
df = []

for i in range(len(fn)):
    df.append(pd.read_csv(fn[i][0], header=3, skipinitialspace=True,
        skipfooter=2, delim_whitespace=True,engine='python'))
    
    ax.plot(df[i]['Operations'].values, df[i]['Ops_per_sec'].values,
            label=fn[i][1])#, s=rcParams['lines.markersize']*2)

#ax[0].set(xlim=(0,200))
ax.legend(loc='upper right')
ax.set_title('Operations per second')
ax.set_xlabel('millions of operations')
ax.set_ylabel('millions of operations / sec')
ax.yaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6)))
ax.xaxis.set_major_formatter(
        ticker.FuncFormatter(lambda x, pos: '{0:g}'.format(x/1e6)))

plt.show()
