from matplotlib.axes import Axes
import numpy as np

class AxesSetting():
    def __init__(self, title="Title", xlabel=None, ylabel=None, xlim=None, ylim=None, xscale="linear", yscale="linear", legend: bool = True, aspect = 'auto'):
        self.title = title
        self.xlabel = xlabel
        self.ylabel = ylabel
        self.xlim = xlim
        self.ylim = ylim
        self.xscale = xscale
        self.yscale = yscale
        self.legend = legend
        self.aspect = aspect
    
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
    
    def plot_func(self, axes: Axes, func, xlim = None, samplesize = 200, c="black", label=None, marker=None, alpha=0.2):
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
        axes.plot(x, func(x), color=c, marker=marker, label=label, alpha=alpha)

def linear_sim(p1: tuple, p2: tuple, y: float) -> float:
    return (-p1[0] * (y - p2[1]) + p2[0] * (y - p1[1])) / (p2[1] - p1[1])