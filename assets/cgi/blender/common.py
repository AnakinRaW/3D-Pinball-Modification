"""Helpers that the scene, the lights, the logo and both films use: colors, collections and keys."""
import bpy


def hexcol(h, a=1.0):
    """A hex color, in sRGB as the drawing writes it, as Blender's linear RGBA."""
    h = h.lstrip("#")
    if len(h) == 3: h = "".join(c * 2 for c in h)
    srgb = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]
    return (*lin, a)


def coll(name):
    """The collection called name, made and linked into the scene where it is missing."""
    sc = bpy.context.scene
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in sc.collection.children: sc.collection.children.link(c)
    return c


def fcurves_of(idblock):
    """The F-curves of a data-block's action, from the slotted actions of Blender 4.4 on as from the older ones."""
    ad = idblock.animation_data
    if not ad or not ad.action: return []
    act = ad.action
    if hasattr(act, "layers") and act.layers:
        out = []
        for layer in act.layers:
            for strip in layer.strips:
                for slot in act.slots:
                    cb = strip.channelbag(slot)
                    if cb: out += list(cb.fcurves)
        return out
    return list(act.fcurves)


def set_interp(idblock, interp):
    for fc in fcurves_of(idblock):
        for kp in fc.keyframe_points: kp.interpolation = interp
