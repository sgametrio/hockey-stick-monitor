import numpy as np
import pandas as pd
from scipy import integrate
gen = np.random.RandomState(0)
x = gen.randn(100, 10)
names = [chr(97 + i) for i in range(10)]
df = pd.DataFrame(x, columns=names)
print(df.head())
df = df.apply(lambda x: np.insert(integrate.cumtrapz(x.values), 0, 0, axis=0))
print(df.head())

