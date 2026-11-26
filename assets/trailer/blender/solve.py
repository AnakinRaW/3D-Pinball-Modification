"""Searches the trailer's game segment by segment and writes it to game.json.

  blender -b --factory-startup --python solve.py

Inside Blender a timer drives steps(), a generator that does one run of the simulation each time it is advanced,
so Blender stays reachable while it searches. Each segment is searched from the state the segment before it ended
in. Where a segment finds no solution, the search goes back one segment and takes its next solution. An optional
segment without a solution sends the search back to the latest segment that has another solution, and it is left
out only once none has. The search never goes back across a cut, since the segment behind a cut starts from a
state of its own choosing, whatever the segments before it did. The parameters keep every digit, since a game is chaotic and a rounded
parameter plays another game.
"""
import json, os, sys, time

HERE = BUILD if "BUILD" in globals() else os.path.dirname(os.path.abspath(__file__))
CGI = os.path.join(os.path.dirname(os.path.dirname(HERE)), "cgi", "blender")
JSON = os.path.join(HERE, "game.json")
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import importlib
import physics as ph
importlib.reload(ph)
import runs as rn
importlib.reload(rn)
import game
importlib.reload(game)


def steps(machine, progress=None, prefix=()):
    """Searches the game; yields after every run, and writes game.json once every segment has a solution. prefix
    gives the first segments' solutions as game.json writes them; they are played, not searched."""
    t_start = time.time()
    stack = []                      # per segment: [solutions, the one taken, the state it starts from]
    state = game.start()
    k, runs = 0, 0
    for entry in prefix:
        seg = game.GAME[k]
        assert seg["name"] == entry["name"], (seg["name"], entry["name"])
        if entry.get("skip"):
            stack.append([[None], 0, state])
        else:
            stack.append([[(entry["params"], entry["goal"])], 0, state])
            c, state, *_ = game.run(machine, seg, state, entry["params"], seg["goals"][entry["goal"]], record=0.05)
            assert c == 0.0, (entry["name"], c)
        k += 1
    while k < len(game.GAME):
        seg = game.GAME[k]
        floor = max((i for i in range(k + 1) if game.GAME[i].get("cut")), default=0)     # the latest cut
        if len(stack) <= k:
            sols = []
            for r in game.search(machine, seg, state, tries=seg.get("tries", 70)):
                if r[0] == "done":
                    sols = r[1]
                    break
                runs += 1
                if progress and runs % 5 == 0:
                    json.dump({"segment": seg["name"], "goal": r[2], "run": r[3], "cost": r[4], "runs": runs,
                               "taken": [game.GAME[i]["name"] for i in range(len(stack))],
                               "s": round(time.time() - t_start)}, open(progress, "w"))
                yield
            if not sols and seg.get("optional"):
                # an optional segment is left out only once no segment before it has another solution to try
                j = max((i for i in range(floor, k) if stack[i][1] + 1 < len(stack[i][0])), default=None)
                if j is not None:
                    del stack[j + 1:]
                    stack[j][1] += 1
                    state, k = stack[j][2], j
                    continue
                sols = [None]
            stack.append([sols, 0, state])
        sols, idx, st0 = stack[k]
        if idx >= len(sols):
            stack.pop()
            if k == 0:
                raise RuntimeError("the game has no start")
            if k == floor:
                raise RuntimeError("segment %s behind its cut has no solution" % seg["name"])
            k -= 1
            stack[k][1] += 1
            state = stack[k][2]
            continue
        if sols[idx] is None:
            state = st0; k += 1
            continue
        q, gi = sols[idx]
        c, state, *_ = game.run(machine, seg, st0, q, seg["goals"][gi], record=0.05)
        k += 1
    out = [{"name": game.GAME[i]["name"], "params": s[0][s[1]][0], "goal": s[0][s[1]][1]}
           for i, s in enumerate(stack) if s[0][s[1]] is not None]
    json.dump(out, open(JSON, "w"), indent=1)
    if progress:
        json.dump({"done": True, "segments": [o["name"] for o in out], "runs": runs,
                   "s": round(time.time() - t_start)}, open(progress, "w"))


if __name__ == "__main__":
    ph.build_scene(CGI)
    for _ in steps(ph.Machine()):
        pass
