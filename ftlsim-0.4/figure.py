# importing libraries
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.axes3d import Axes3D
import numpy as np
from pylab import meshgrid


def wl_generalbound(sf, r, f, Th):
    res = (2.0 * (1-sf) * (r/(1-sf) - f)) / ((Th+1) * (1/(1-sf) - f))
    # res = (2.0 * (1 - sf) * (r - f)) / ((Th + 1) * (1 - f))
    return res


r = np.arange(0, 1, 0.01)
f = np.arange(0, 1, 0.01)
X, Y = meshgrid(r, f)
Z = wl_generalbound(0.05, X, Y, 3)
# set all values outside condition to nan
Z[X <= Y] = np.nan
fig = plt.figure()
ax = Axes3D(fig)
surf = ax.plot_surface(X, Y, Z)

ax.set_xlabel('r')
ax.set_ylabel('f')
ax.set_zlabel('AmplificationFactor_WL')
# ax.view_init(elev=15, azim=120)
ax.set_zlim(0, 0.5)
ax.view_init(elev=13, azim=-150)
plt.show()

