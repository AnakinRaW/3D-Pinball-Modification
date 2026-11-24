"""Exports the machine as machine.glb in the cgi folder, for the Arcade's 3D view.

Run inside Blender with BUILD set to this folder. It builds the machine with scene.py and exports everything under
machine-tilt, the backbox and the diorama's ceiling strip, with the tilt and the modifiers applied. The parts of
each placement of the drawing are joined into one node, which carries the placement's id as the glTF extra
drawing_id. One glTF unit is one meter,
and glTF's Y axis points up. The meshes stay uncompressed: meshopt's position filter rounds a height of 0.02 mm
next to 200 mm across onto the board's, and parts laid a hair above the board then flicker in a viewer.
"""
import bpy, json, math, mathutils, os

ASSETS = os.path.dirname(os.path.dirname(BUILD))
GLB = os.path.join(os.path.dirname(BUILD), "machine.glb")
OUT = os.path.join(BUILD, "out")
TEXTURE_HEIGHT = 2048          # px, the height every texture is scaled to
JPEG_QUALITY = 85
TILE_HEIGHT = 256              # px: a texture this small is a repeating tile
# the backbox stands upright behind the board and the cabinet's outer panels hang beside and before it, where the
# drawing's top view places nothing, so their parts go by name; so do the parts that reach well past their
# placement's outline in the drawing, or lie under the board
NAMED_IDS = (("backbox-b20", "dio-b20"), ("backbox-scene", "dio-scene"), ("backbox-back", "sky"),
             ("display-screen", "display"), ("backbox-frame", "dio-frame"), ("diorama-", "dio-frame"),
             ("lamp-ceiling", "dio-frame"), ("front-", "cabinet"), ("side-", "cabinet"), ("playfield", "cabinet"), ("cabinet-", "cabinet"),
             ("plunger-", "shooter-lane-floor"), ("shooter-floor", "shooter-lane-floor"),
             ("separator-left", "separator-left"), ("separator-right", "separator-right"),
             ("crocodile-", "ramp-kicker"), ("store-", "drain-ball-store"), ("lane-", "ramp-blue"),
             ("deco-spinner-", "lane-guides-left"))
# parts that come in a left and a right copy go by name and by the side of the board they stand on
SIDED_IDS = (("sling-rubber", ("slingshot-left", "slingshot-right")),)
MIDDLE_X = 108.5               # mm, the board's middle in the drawing
# a part held by the cabinet's outline only, and partly by a smaller shape, goes to the smaller shape holding most
# of it, when that holds at least this share
FALLBACK_SHARE = 0.2

exec(compile(open(os.path.join(BUILD, "scene.py"), encoding="utf-8").read(), "scene.py", "exec"),
     {"BUILD": BUILD, "__name__": "scene"})
tilt = bpy.data.objects["machine-tilt"]


def placement_ids():
    """Every id the drawing gives a placement, from playfield.json."""
    ids = set()

    def walk(n):
        if isinstance(n, dict):
            if "id" in n and "part" in n:
                ids.add(n["id"])
            for v in n.values():
                walk(v)
        elif isinstance(n, list):
            for v in n:
                walk(v)
    walk(json.load(open(os.path.join(ASSETS, "playfield", "playfield.json"), encoding="utf-8"))["layers"])
    return ids


def inside(x, y, pts):
    hit = False
    for (x1, y1), (x2, y2) in zip(pts[-1:] + pts[:-1], pts):
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            hit = not hit
    return hit


def near(x, y, pts, closed, reach):
    segs = zip(pts, pts[1:] + (pts[:1] if closed else []))
    for (x1, y1), (x2, y2) in segs:
        dx, dy = x2 - x1, y2 - y1
        t = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy or 1.0)))
        if math.hypot(x - x1 - t * dx, y - y1 - t * dy) <= reach:
            return True
    return False


def shapes():
    """The drawing's shapes with the innermost placement of each: (id, points, closed and filled, reach in mm,
    area in mm², bounding box)."""
    ids = placement_ids()
    out = []
    for s in json.load(open(os.path.join(OUT, "shapes.json")))["shapes"]:
        own = [c for c in s["chain"] if c in ids]
        pts = [tuple(p) for p in s["pts"]]
        if not own or len(pts) < 2:
            continue
        filled = s["closed"] and s.get("fill") not in (None, "none")
        reach = s["sw"] / 2 + 0.6
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        if filled:
            area = abs(sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(pts, pts[1:] + pts[:1]))) / 2
        else:
            area = sum(math.dist(a, b) for a, b in zip(pts, pts[1:])) * 2 * reach
        out.append((own[-1], pts, filled, reach, area, (min(xs) - reach, max(xs) + reach, min(ys) - reach, max(ys) + reach)))
    return out


def footprint(o, dg, inv):
    """Up to 48 of the object's evaluated vertices in the drawing's millimeters, (x, y)."""
    ev = o.evaluated_get(dg)
    me = ev.to_mesh()
    m = inv @ o.matrix_world
    vs = me.vertices
    step = max(1, len(vs) // 48)
    pts = [(lambda q: (q.x * 10, -q.y * 10))(m @ vs[i].co) for i in range(0, len(vs), step)]
    ev.to_mesh_clear()
    return pts


def tag(objects):
    """Gives every object the drawing id of the smallest shape that holds at least half of its footprint; the
    backbox's and the outer panels' parts go by name. Returns the names of the objects no shape holds."""
    table = shapes()
    dg = bpy.context.evaluated_depsgraph_get()
    inv = tilt.matrix_world.inverted()
    loose = []
    for o in objects:
        named = next((pid for prefix, pid in NAMED_IDS if o.name.startswith(prefix)), None)
        if named:
            o["drawing_id"] = named
            continue
        pts = footprint(o, dg, inv)
        if not pts:
            loose.append(o.name)
            continue
        sided = next((pair for prefix, pair in SIDED_IDS if o.name.startswith(prefix)), None)
        if sided:
            o["drawing_id"] = sided[0] if sum(p[0] for p in pts) / len(pts) < MIDDLE_X else sided[1]
            continue
        x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
        y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
        best, most = None, None
        for pid, poly, filled, reach, area, (bx0, bx1, by0, by1) in table:
            if bx1 < x0 or bx0 > x1 or by1 < y0 or by0 > y1:
                continue
            hits = sum(1 for x, y in pts if (filled and inside(x, y, poly)) or near(x, y, poly, filled, reach))
            if hits * 2 >= len(pts) and (best is None or area < best[0]):
                best = (area, pid)
            if pid != "cabinet" and hits and (most is None or hits > most[0] or hits == most[0] and area < most[1]):
                most = (hits, area, pid)
        if (best is None or best[1] == "cabinet") and most and most[0] >= FALLBACK_SHARE * len(pts):
            best = (most[1], most[2])
        if best:
            o["drawing_id"] = best[1]
        else:
            loose.append(o.name)
    return loose


def flatten_materials(materials):
    """Leaves each material base color, roughness, metallic, emission and alpha: a color mixed by nodes becomes
    the mean of its two colors, and transmission becomes alpha."""
    for m in materials:
        if not m.use_nodes:
            continue
        b = m.node_tree.nodes.get("Principled BSDF")
        if b is None:
            continue
        base = b.inputs["Base Color"]
        if base.is_linked and base.links[0].from_node.type == "MIX":
            mix = base.links[0].from_node
            a, c = mix.inputs[6].default_value, mix.inputs[7].default_value
            m.node_tree.links.remove(base.links[0])
            base.default_value = tuple((a[i] + c[i]) / 2 for i in range(3)) + (1.0,)
        t = b.inputs["Transmission Weight"].default_value
        if t > 0:
            b.inputs["Transmission Weight"].default_value = 0.0
            b.inputs["Alpha"].default_value = min(b.inputs["Alpha"].default_value, 1.0 - 0.6 * t)
        if b.inputs["Alpha"].is_linked or b.inputs["Alpha"].default_value < 1.0:
            m.surface_render_method = "BLENDED"
        b.inputs["Coat Weight"].default_value = 0.0


def shrink_images(materials):
    """Scales every texture taller than TEXTURE_HEIGHT down to it; one without transparent pixels is stored as a
    JPEG. A small tile goes as it is, since a JPEG would blur its sharp edges."""
    done = {}
    for m in materials:
        if not m.use_nodes:
            continue
        for n in m.node_tree.nodes:
            if n.type != "TEX_IMAGE" or n.image is None:
                continue
            img = n.image
            if img.name not in done and img.size[1] <= TILE_HEIGHT:
                done[img.name] = img                  # a small tile goes as it is, a PNG
            if img.name not in done:
                w, h = img.size
                small = img.copy()
                if h > TEXTURE_HEIGHT:
                    small.scale(max(1, round(w * TEXTURE_HEIGHT / h)), TEXTURE_HEIGHT)
                alpha = n.outputs["Alpha"].is_linked
                ext = "png" if alpha else "jpg"
                path = os.path.join(OUT, "export-" + os.path.splitext(img.name)[0] + "." + ext)
                small.filepath_raw = path
                small.file_format = "PNG" if alpha else "JPEG"
                small.save(quality=JPEG_QUALITY)
                done[img.name] = bpy.data.images.load(path)
            n.image = done[img.name]


def export():
    keep = [o for o in bpy.data.objects
            if o.type in ("MESH", "CURVE") and not o.hide_render
            and (o.parent is not None and _under(o, tilt) or any(o.name.startswith(p) for p, _ in NAMED_IDS))]
    loose = tag(keep)
    materials = {s.material for o in keep for s in o.material_slots if s.material}
    flatten_materials(materials)
    shrink_images(materials)
    parts = merge(keep)
    # one glTF unit is a meter: a root scales the 10 mm Blender unit down
    root = bpy.data.objects.new("machine", None)
    bpy.context.scene.collection.objects.link(root)
    root.scale = (0.01, 0.01, 0.01)
    for o in [tilt] + [o for o in parts if o.parent is None]:
        o.parent = root
        o.matrix_parent_inverse.identity()
    bpy.ops.object.select_all(action="DESELECT")
    for o in [root, tilt] + parts:
        o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=GLB, export_format="GLB", use_selection=True, export_apply=True,
                              export_extras=True, export_yup=True, export_cameras=False, export_lights=False,
                              export_animations=False, export_image_format="AUTO", export_jpeg_quality=JPEG_QUALITY,
                              export_meshopt_compression_enable=False)
    return {"objects": len(keep), "parts": len(parts), "untagged": loose, "bytes": os.path.getsize(GLB)}


def merge(objects):
    """Joins the objects of each drawing id into one mesh, with their modifiers applied and a primitive for each
    material, so the file carries one node per placement. An object without an id stays a mesh of its own."""
    dg = bpy.context.evaluated_depsgraph_get()
    inv = tilt.matrix_world.inverted()
    groups = {}
    for o in objects:
        under = _under(o, tilt)
        groups.setdefault((o.get("drawing_id") or o.name, under), []).append(o)
    parts = []
    for (key, under), objs in sorted(groups.items()):
        pieces = []
        for o in objs:
            me = bpy.data.meshes.new_from_object(o.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
            me.transform((inv if under else mathutils.Matrix.Identity(4)) @ o.matrix_world)
            piece = bpy.data.objects.new(key, me)
            bpy.context.scene.collection.objects.link(piece)
            if under:
                piece.parent = tilt
            pieces.append(piece)
        # joining keeps only UV maps of the same name, so where one piece has a map every piece carries one named
        # alike
        for piece in pieces if any(p.data.uv_layers for p in pieces) else []:
            uv = piece.data.uv_layers
            if not uv:
                uv.new(name="UVMap")
            uv[0].name = "UVMap"
            for extra in list(uv)[1:]:
                uv.remove(extra)
        if len(pieces) > 1:
            with bpy.context.temp_override(active_object=pieces[0], selected_editable_objects=pieces):
                bpy.ops.object.join()
        part = pieces[0]
        part.name = key
        if objs[0].get("drawing_id"):
            part["drawing_id"] = objs[0]["drawing_id"]
        parts.append(part)
    return parts


def _under(o, top):
    p = o.parent
    while p:
        if p == top:
            return True
        p = p.parent
    return False


result = export()
# the export rescaled, joined and flattened the scene; the model is built again as scene.py leaves it
exec(compile(open(os.path.join(BUILD, "scene.py"), encoding="utf-8").read(), "scene.py", "exec"),
     {"BUILD": BUILD, "__name__": "scene"})
