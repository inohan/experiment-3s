from matplotlib.axes import Axes
import numpy as np

class AxesSetting():
    def __init__(self, title=None, xlabel=None, ylabel=None, xlim=None, ylim=None, xscale="linear", yscale="linear", legend: bool = True, aspect = 'auto', grid=None):
        self.title = title
        self.xlabel = xlabel
        self.ylabel = ylabel
        self.xlim = xlim
        self.ylim = ylim
        self.xscale = xscale
        self.yscale = yscale
        self.legend = legend
        self.aspect = aspect
        self.grid = {"which": "major", "axis": None}
        if (type(grid) is tuple or type(grid) is list) and len(grid) == 2:
            self.grid = {"which": grid[0], "axis": grid[1]}
        elif type(grid) is dict:
            if "which" in grid.keys():
                self.grid["which"] = grid["which"]
            if "axis" in grid.keys():
                self.grid["axis"] = grid["axis"]
        else:
            self.grid["axis"] = grid
    
    def apply_axes(self, axes: Axes):
        if self.title:
            axes.set_title(self.title)
        if self.xlabel:
            axes.set_xlabel(self.xlabel)
        if self.ylabel:
            axes.set_ylabel(self.ylabel)
        if self.xlim:
            axes.set_xlim(self.xlim)
        if self.ylim:
            axes.set_ylim(self.ylim)
        if self.xscale:
            axes.set_xscale(self.xscale)
        if self.yscale:
            axes.set_yscale(self.yscale)
        if self.legend:
            axes.legend()
        if self.aspect:
            axes.set_aspect(self.aspect)
        if self.grid["axis"] is not None:
            axes.grid(True, self.grid["which"], self.grid["axis"])
    
    def plot_func(self, axes: Axes, func, xlim = None, samplesize = 200, **kwargs):
        x = None
        if xlim:
            if self.xscale == "log":
                x = np.geomspace(xlim[0], xlim[1], samplesize)
            else:
                x = np.linspace(xlim[0], xlim[1], samplesize)
        elif self.xlim:
            if self.xscale == "log":
                x = np.geomspace(self.xlim[0], self.xlim[1], samplesize)
            else:
                x = np.linspace(self.xlim[0], self.xlim[1], samplesize)
        else:
            raise Exception("Limit not defined.")
        axes.plot(x, func(x), **kwargs)

def linear_sim(p1: tuple, p2: tuple, y: float) -> float:
    return (-p1[0] * (y - p2[1]) + p2[0] * (y - p1[1])) / (p2[1] - p1[1])

def linear_approx(x, y, rng, start = 0.5, fmt=None):
    assert len(x) == len(y)
    assert len(rng) == 2
    coords = [(x[i], y[i]) for i in range(len(x))]
    coords.sort(key=lambda coord: coord[0])
    filtered = [coord for coord in coords if coord[0] >= rng[0] and coord[0] <= rng[1]]
    if len(filtered) < 2:
        raise ValueError(f"More than 2 points must be in the range [{rng[0]}, {rng[1]}], but found {len(filtered)}")
    slope = (filtered[-1][1] - filtered[0][1])/(filtered[-1][0] - filtered[0][0])
    startx = start * filtered[-1][0] + (1-start) * filtered[0][0]
    starty = start * filtered[-1][1] + (1-start) * filtered[0][1]
    if fmt:
        return fmt.format(slope=slope, slice=-slope*startx+starty)
    else:
        return lambda x: slope * (x-startx) + starty
