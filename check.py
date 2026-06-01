import pandas as pd
import numpy as np

df = pd.read_csv('gesture_data.csv')
print('CSV sample (forward):')
print(df[df['label']=='forward'].iloc[0, :6].values)
print('CSV sample (stop):')
print(df[df['label']=='stop'].iloc[0, :6].values)