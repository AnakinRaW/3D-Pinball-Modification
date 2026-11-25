"""What game.py's goals are made of: the machine's places the game passes and the gates a ball has to pass.

Positions are millimeters of the drawing, speeds mm/s, times seconds.
"""
from mathutils import Vector
import physics as ph

SCOOP = (31.87, 123.66)
DRAIN = (108.54, 387.04)
PLUNGER = (224.4, 412.83, 5.69)    # the ball resting on the plunger's rod tip, where a ball dropped into the lane settles


def at(x, y, z=4.6):
    return ph.mm(x, y, z)


def vel(vx, vy, vz=0.0):
    """A velocity in mm/s of the drawing, y down the board."""
    return Vector((vx / 10.0, -vy / 10.0, vz / 10.0))


class Gate:
    """A point a ball has to come within reach of, in the board's plane, optionally only below or above a
    height, or a kind of event it has to have, at that point where one is given. ball names the ball; without it
    any ball in play passes."""

    def __init__(self, name, x=None, y=None, reach=4.0, zmax=None, zmin=None, event=None, ball=None):
        self.name, self.x, self.y, self.reach = name, x, y, reach
        self.zmax, self.zmin, self.event, self.ball = zmax, zmin, event, ball


def seal_turn(samples, events, t):
    """The way the ball's hit at t turns the seal: +1 clockwise on the board seen from above, -1 the other way.
    The rotor takes the impulse the ball loses, at the ball's place against the seal's pivot."""
    s = samples[0]
    before = min(s, key=lambda x: abs(x[0] - (t - 0.004)))
    after = min(s, key=lambda x: abs(x[0] - (t + 0.004)))
    push = before[2] - after[2]
    r = before[1] - ph.mm(120.43, 169.65)
    torque = r.x * push.y - r.y * push.x          # counter-clockwise positive in the board's frame
    return -1 if torque > 0 else 1
