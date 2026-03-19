import pandas as pd
import random

df = pd.read_csv('dataset.csv')
df.columns = [c.strip().lower() for c in df.columns]

s0 = df[df['label'] == 0].sample(17, random_state=42)
s1 = df[df['label'] == 1].sample(17, random_state=42)
s2 = df[df['label'] == 2].sample(16, random_state=42)

samples = pd.concat([s0, s1, s2])

print("const float test_dataset[][3] = {")
for idx, row in samples.iterrows():
    print(f"    {{{row['temperature']:.1f}, {row['humidity']:.1f}, {int(row['label'] )}}},")
print("};")
