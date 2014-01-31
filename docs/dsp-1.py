import matplotlib.pyplot as plt
import numpy as np
import dds
i, q = dds.dds_lut(10, 256, 0.8)
n = np.arange(256)
plt.plot(n, i, n, q)