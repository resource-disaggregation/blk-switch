import pandas as pd
import sys

# ./final-results/spdkpp-t1p-lat4.sched.log
df = pd.read_csv(sys.argv[1], sep='\s+', names=['time', 'coremask', 'thread', 'wait', 'sched', 'run'])

dff = df[df['thread'].str.contains('perf')]
dff['block'] = (dff['thread'] != dff['thread'].shift()).cumsum()

import numpy as np
def percentile(n):
    def percentile_(x):
        return np.percentile(x, n)
    percentile_.__name__ = 'percentile_%s' % n
    return percentile_

print('\nRun-time\n')
print(dff.groupby('block').agg({'thread': 'first', 'run': 'sum'}).groupby('thread').agg([percentile(25), percentile(50), percentile(75),'mean', 'count']))

def compute_gaps(y):
    x = y.sort_values(by=['time'])
    x['time_prev'] = x['time'].shift()
    x['wait'] = x['time']*1000 - x['time_prev']*1000 - x['run']
    return x['wait'].mean()

print('\nWait-time\n')
print(dff.groupby('block').agg({'thread': 'first', 'time': 'max', 'run': 'sum'}).groupby('thread').apply(compute_gaps))