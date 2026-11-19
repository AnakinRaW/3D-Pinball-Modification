"""Reports which rows of docs/firmware/testing.md have a test in the code.

A row of a program's table counts as covered when the code holds a test of that name that runs where
the row says:

    computer          the test function test<Row> of the computer test test_<program>, such as
                      testOldestFirst for the row oldest-first
    Teensy, bench     {"<program>.<row>", RunsOn::TEENSY} or RunsOn::BENCH in the folder
                      firmware/tests/teensy/src/<program>/ of the test image
    machine           {"<program>.<row>", RunsOn::MACHINE} in the same place

With --computer-results, the computer tests are those that ran, from the JSON report of
`pio test -e computer --json-output-path <file>`. Without it they are read from the RUN_TEST() calls
in firmware/tests/test_<program>/. The image's tests are read from its sources, with every comment
removed first, and count only in a program that firmware/tests/teensy/src/main.cpp lists in
PROGRAMS.

The run fails when the code and the document disagree:

    a test in the code that has no row, or a row and a test named twice
    a test that runs somewhere other than its row says
    a test of the image outside its program's folder, or in a program PROGRAMS does not list
    a computer test of a program that testing.md does not hold
    a line of a test table in testing.md that does not read as a row

A row without a test is only counted against the coverage.

    python firmware/tools/test-coverage.py              the coverage of every program
    python firmware/tools/test-coverage.py --missing    with the name of every row without a test
    python firmware/tools/test-coverage.py --computer-results report.json
    python firmware/tools/test-coverage.py --self-test   breaks a copy each way the check guards

The report goes to the console. In GitHub Actions, a table of every program also goes to the job's
summary page.

Exit code 0 when the code and the document agree, 1 when they disagree. With --self-test, 0 when
every broken copy got the verdict it should, 1 otherwise.
"""

import argparse
import glob
import json
import os
import re
import shutil
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
TESTING_MD = os.path.join(ROOT, "docs", "firmware", "testing.md")
TESTS_DIR = os.path.join(ROOT, "firmware", "tests")
IMAGE_SRC = os.path.join(TESTS_DIR, "teensy", "src")
IMAGE_MAIN = os.path.join(IMAGE_SRC, "main.cpp")

PLACES = ["computer", "Teensy", "bench", "machine"]
# the RunsOn value of the test image that stands for each place of testing.md
RUNS_ON = {"TEENSY": "Teensy", "BENCH": "bench", "MACHINE": "machine"}

PROGRAM_LINE = re.compile(r"^The program `([a-z0-9-]+)`")
TABLE_HEADER = re.compile(r"^\| Test \| Runs on \|")
ROW = re.compile(r"^\| `([a-z0-9-]+)` \| ([A-Za-z]+) \|")
RUN_TEST = re.compile(r"\bRUN_TEST\(\s*(\w+)\s*\)")
IMAGE_TEST = re.compile(r'\{\s*"([a-z0-9-]+)\.([a-z0-9-]+)"\s*,\s*RunsOn::(\w+)')
TABLE_RULE = re.compile(r"^\|(\s*:?-+:?\s*\|)+\s*$")
PROGRAMS = re.compile(r"\bPROGRAMS\s*\[\s*\]\s*=\s*\{([^}]*)\}")
PROGRAM_DEFINITION = re.compile(r"\bextern\s+const\s+Program\s+(\w+)\s*=")


class Report:
    def __init__(self):
        self.errors = []

    def error(self, where, text):
        self.errors.append(f"{where}: {text}")


def relative(path):
    return os.path.relpath(path, ROOT).replace(os.sep, "/")


def without_comments(source):
    """Gets C++ source with every comment blanked out, string literals kept, and lines in place."""
    out = []
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if c in "\"'":
            j = i + 1
            while j < n and source[j] != c:
                j += 2 if source[j] == "\\" else 1
            out.append(source[i:j + 1])
            i = j + 1
        elif source.startswith("//", i):
            j = source.find("\n", i)
            j = n if j < 0 else j
            i = j
        elif source.startswith("/*", i):
            j = source.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * source.count("\n", i, j))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def read_source(path):
    with open(path, encoding="utf-8") as f:
        return without_comments(f.read())


def read_rows(report):
    """Reads every program's rows from testing.md, in the order of the document.

    Gets a list of (program, line, rows), where rows maps a row's name to (place, line).
    """
    programs = []
    current = None
    in_table = False
    where = relative(TESTING_MD)
    with open(TESTING_MD, encoding="utf-8") as f:
        for number, text in enumerate(f, start=1):
            program = PROGRAM_LINE.match(text)
            if program:
                current = (program.group(1), number, {})
                programs.append(current)
                in_table = False
                continue
            if TABLE_HEADER.match(text):
                in_table = current is not None
                continue
            if not text.startswith("|"):
                in_table = False
                continue
            if not in_table or TABLE_RULE.match(text):
                continue
            row = ROW.match(text)
            if not row:
                report.error(f"{where}:{number}", f"a line of the program {current[0]}'s table "
                             "reads as no row: | `name` | place | ...")
                continue
            name, place = row.group(1), row.group(2)
            if place not in PLACES:
                report.error(f"{where}:{number}", f"{current[0]}.{name} runs on '{place}', which "
                             f"is none of {', '.join(PLACES)}")
                continue
            if name in current[2]:
                report.error(f"{where}:{number}", f"{current[0]}.{name} stands twice")
                continue
            current[2][name] = (place, number)
    seen = {}
    for name, number, _ in programs:
        if name in seen:
            report.error(f"{where}:{number}", f"the program {name} stands twice")
        seen[name] = number
    return programs


def computer_function(row):
    """Gets the name of the computer test function for a row, such as testLayout3a for layout-3a."""
    return "test" + "".join(part[:1].upper() + part[1:] for part in row.split("-"))


def read_computer_tests(report, results):
    """Gets every computer test as a map from (program, function) to where it stands.

    The tests come from the JSON report of a computer test run where one is given, and from the
    RUN_TEST() calls in the sources otherwise.
    """
    tests = {}

    def add(program, function, where):
        if (program, function) in tests:
            report.error(where, f"{function} runs twice")
        else:
            tests[(program, function)] = where

    if results:
        with open(results, encoding="utf-8") as f:
            run = json.load(f)
        for suite in run["test_suites"]:
            if suite["env_name"] != "computer":
                continue
            program = suite["test_name"][len("test_"):]
            for case in suite["test_cases"]:
                source = case.get("source") or {}
                where = f"{source.get('file', suite['test_name'])}:{source.get('line', '?')}"
                add(program, case["name"], where)
        return tests

    for folder in sorted(glob.glob(os.path.join(TESTS_DIR, "test_*"))):
        program = os.path.basename(folder)[len("test_"):]
        for path in sorted(glob.glob(os.path.join(folder, "**", "*.cpp"), recursive=True)):
            source = read_source(path)
            for match in RUN_TEST.finditer(source):
                number = source.count("\n", 0, match.start()) + 1
                add(program, match.group(1), f"{relative(path)}:{number}")
    return tests


def read_image_tests(report):
    """Gets every test of the image as a map from (program, row) to (place, where it stands)."""
    listed = PROGRAMS.search(read_source(IMAGE_MAIN))
    if not listed:
        report.error(relative(IMAGE_MAIN), "holds no PROGRAMS list")
        return {}
    registered = set(re.findall(r"&\s*(\w+)", listed.group(1)))

    tests = {}
    for path in sorted(glob.glob(os.path.join(IMAGE_SRC, "**", "*.cpp"), recursive=True)):
        if os.path.samefile(path, IMAGE_MAIN):
            continue
        source = read_source(path)
        found = list(IMAGE_TEST.finditer(source))
        if not found:
            continue
        folder = os.path.relpath(path, IMAGE_SRC).replace(os.sep, "/").split("/")[0]
        definitions = PROGRAM_DEFINITION.findall(source)
        unlisted = [name for name in definitions if name not in registered]
        if not definitions or unlisted:
            report.error(relative(path), f"its tests do not run: {', '.join(unlisted) or 'no Program'}"
                         f" is missing from PROGRAMS in {relative(IMAGE_MAIN)}")
            continue
        for match in found:
            program, row, runs_on = match.groups()
            number = source.count("\n", 0, match.start()) + 1
            where = f"{relative(path)}:{number}"
            if program != folder:
                report.error(where, f"{program}.{row} stands in the folder {folder}")
                continue
            if runs_on not in RUNS_ON:
                report.error(where, f"{program}.{row} runs on RunsOn::{runs_on}, which stands for "
                             "no place of testing.md")
                continue
            if (program, row) in tests:
                report.error(where, f"{program}.{row} stands twice in the image")
                continue
            tests[(program, row)] = (RUNS_ON[runs_on], where)
    return tests


def compare(programs, computer, image, report):
    """Marks every row covered or missing, and reports every test of the code that has no row.

    Gets a list of (program, covered, rows) with covered the set of covered row names.
    """
    where_md = relative(TESTING_MD)
    results = []
    known = {name for name, _, _ in programs}
    used_computer = set()
    used_image = set()
    for program, _, rows in programs:
        covered = set()
        for row, (place, number) in rows.items():
            if place == "computer":
                key = (program, computer_function(row))
                if key in computer:
                    covered.add(row)
                    used_computer.add(key)
                if (program, row) in image:
                    other, where = image[(program, row)]
                    report.error(where, f"{program}.{row} runs on {other} in the image, and "
                                 f"{where_md}:{number} gives computer")
                    used_image.add((program, row))
            else:
                if (program, row) in image:
                    used_image.add((program, row))
                    other, where = image[(program, row)]
                    if other == place:
                        covered.add(row)
                    else:
                        report.error(where, f"{program}.{row} runs on {other}, and "
                                     f"{where_md}:{number} gives {place}")
                if (program, computer_function(row)) in computer:
                    where = computer[(program, computer_function(row))]
                    report.error(where, f"{program}.{row} runs on the computer, and "
                                 f"{where_md}:{number} gives {place}")
                    used_computer.add((program, computer_function(row)))
        results.append((program, covered, rows))

    for (program, function), where in sorted(computer.items()):
        if (program, function) in used_computer:
            continue
        if program not in known:
            report.error(where, f"the folder test_{program} names no program of {where_md}")
        else:
            report.error(where, f"{function} is no computer row of the program {program}")
    for (program, row), (place, where) in sorted(image.items()):
        if (program, row) not in used_image:
            report.error(where, f"{program}.{row} is no row of {where_md}")
    return results


def percent(part, whole):
    return f"{100 * part / whole:.0f} %" if whole else "-"


def count(results, place):
    """Gets how many rows of a place the code covers, and how many there are."""
    covered = total = 0
    for _, done, rows in results:
        for row, (where, _) in rows.items():
            if place in (None, where):
                total += 1
                covered += row in done
    return covered, total


def console(results, missing):
    width = max(len(program) for program, _, _ in results)
    for program, covered, rows in results:
        print(f"{program:<{width}}  {len(covered):>3} of {len(rows):>3}  "
              f"{percent(len(covered), len(rows)):>5}")
        if missing:
            for row, (place, _) in rows.items():
                if row not in covered:
                    print(f"{'':<{width}}  missing {row} ({place})")
    print()
    for place in PLACES + [None]:
        covered, total = count(results, place)
        label = place or "all"
        print(f"{label:<{width}}  {covered:>3} of {total:>3}  {percent(covered, total):>5}")


def summary(results):
    lines = ["## Coverage of testing.md", ""]
    lines.append("| Program | " + " | ".join(PLACES) + " | All |")
    lines.append("|---|" + "---:|" * (len(PLACES) + 1))
    for program, covered, rows in results:
        cells = []
        for place in PLACES + [None]:
            names = [row for row, (where, _) in rows.items() if place in (None, where)]
            done = sum(row in covered for row in names)
            cells.append(f"{done} of {len(names)}" if names else "")
        lines.append(f"| `{program}` | " + " | ".join(cells) + " |")
    cells = []
    for place in PLACES + [None]:
        covered, total = count(results, place)
        cells.append(f"**{covered} of {total}, {percent(covered, total)}**")
    lines.append("| All | " + " | ".join(cells) + " |")
    return "\n".join(lines) + "\n"


def evaluate(computer_results=None):
    """Runs the check on the files the module's paths name. Gets (results, report)."""
    report = Report()
    programs = read_rows(report)
    computer = read_computer_tests(report, computer_results)
    results = compare(programs, computer, read_image_tests(report), report)
    return results, report


def report_from_sources(path):
    """Writes the JSON report a computer test run would give, from the RUN_TEST() calls."""
    suites = {}
    for (program, function), where in read_computer_tests(Report(), None).items():
        file, line = where.rsplit(":", 1)
        suites.setdefault(program, []).append(
            {"name": function, "status": "PASSED", "source": {"file": file, "line": int(line)}})
    run = {"test_suites": [{"env_name": "computer", "test_name": f"test_{program}",
                            "test_cases": cases} for program, cases in suites.items()]}
    with open(path, "w", encoding="utf-8") as f:
        json.dump(run, f)


def self_test():
    """Breaks a copy of testing.md and the tests one way at a time and checks each verdict."""
    global TESTING_MD, TESTS_DIR, IMAGE_SRC, IMAGE_MAIN
    paths = TESTING_MD, TESTS_DIR, IMAGE_SRC, IMAGE_MAIN
    base_results, base_report = evaluate()
    if base_report.errors:
        print("the self-test needs a run without errors first")
        return 1
    base = count(base_results, None)[0]
    teensy = next(((program, row) for program, covered, rows in base_results
                   for row in sorted(covered) if rows[row][0] == "Teensy"), None)
    computer = next(((program, row) for program, covered, rows in base_results
                     for row in sorted(covered) if rows[row][0] == "computer"), None)
    if teensy is None or computer is None:
        print("the self-test needs a covered Teensy row and a covered computer row")
        return 1
    moved = next(((program, row) for program, covered, rows in base_results if program != teensy[0]
                  for row in sorted(covered) if rows[row][0] == "Teensy"), None)
    if moved is None:
        print("the self-test needs covered Teensy rows in two programs")
        return 1

    def edit(path, old, new):
        with open(path, encoding="utf-8") as f:
            text = f.read()
        if old not in text:
            raise AssertionError(f"{old!r} not found in {path}")
        with open(path, "w", encoding="utf-8") as f:
            f.write(text.replace(old, new, 1))

    def image_file(program):
        return os.path.join(IMAGE_SRC, program, f"{program}.cpp")

    def computer_file(program):
        return glob.glob(os.path.join(TESTS_DIR, f"test_{program}", "**", "*.cpp"),
                         recursive=True)

    def comment_run_test(program, function):
        for path in computer_file(program):
            with open(path, encoding="utf-8") as f:
                if f"RUN_TEST({function})" in f.read():
                    edit(path, f"RUN_TEST({function})", f"// RUN_TEST({function})")
                    return
        raise AssertionError(f"RUN_TEST({function}) not found")

    def drop_case(report_path, function):
        with open(report_path, encoding="utf-8") as f:
            run = json.load(f)
        for suite in run["test_suites"]:
            suite["test_cases"] = [c for c in suite["test_cases"] if c["name"] != function]
        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(run, f)

    entry = f'{{"{teensy[0]}.{teensy[1]}", RunsOn::TEENSY'
    function = computer_function(computer[1])
    symbol = None
    with open(image_file(teensy[0]), encoding="utf-8") as f:
        symbol = PROGRAM_DEFINITION.search(without_comments(f.read())).group(1)

    def move_test(report_path):
        moved_entry = f'{{"{moved[0]}.{moved[1]}", RunsOn::TEENSY'
        edit(image_file(moved[0]), moved_entry, "// " + moved_entry)
        edit(image_file(teensy[0]), entry, moved_entry)

    # each case: its name, how it breaks the copy, whether the run reads the report, and the
    # verdict: the count of covered rows, or "error"
    cases = [
        ("nothing broken", lambda r: None, False, base),
        ("nothing broken, from the report", lambda r: None, True, base),
        ("an image test in a comment", lambda r: edit(image_file(teensy[0]), entry, "// " + entry),
         False, base - 1),
        ("a RUN_TEST() in a comment", lambda r: comment_run_test(computer[0], function), False,
         base - 1),
        ("a computer test the report does not list", lambda r: drop_case(r, function), True,
         base - 1),
        ("a RUN_TEST() in a comment that the report lists",
         lambda r: comment_run_test(computer[0], function), True, base),
        ("an image test that runs on the bench, its row on the Teensy",
         lambda r: edit(image_file(teensy[0]), entry,
                        f'{{"{teensy[0]}.{teensy[1]}", RunsOn::BENCH'), False, "error"),
        ("an image test that has no row",
         lambda r: edit(image_file(teensy[0]), entry, f'{{"{teensy[0]}.no-such-row", RunsOn::TEENSY'),
         False, "error"),
        ("a computer test that has no row",
         lambda r: edit(computer_file(computer[0])[0], f"RUN_TEST({function})",
                        f"RUN_TEST({function});\n    RUN_TEST(testNoSuchRow)"), False, "error"),
        ("a program missing from PROGRAMS",
         lambda r: edit(IMAGE_MAIN, f"&{symbol}", "nullptr"), False, "error"),
        ("an image test in another program's folder", move_test, False, "error"),
        ("a table line of testing.md that reads as no row",
         lambda r: edit(TESTING_MD, f"| `{computer[1]}` |", f"| {computer[1]} |"), False, "error"),
    ]

    failed = 0
    for name, breaks, from_report, wanted in cases:
        work = tempfile.mkdtemp()
        try:
            TESTS_DIR = os.path.join(work, "tests")
            shutil.copytree(paths[1], TESTS_DIR)
            TESTING_MD = os.path.join(work, "testing.md")
            shutil.copy(paths[0], TESTING_MD)
            IMAGE_SRC = os.path.join(TESTS_DIR, "teensy", "src")
            IMAGE_MAIN = os.path.join(IMAGE_SRC, "main.cpp")
            report_path = os.path.join(work, "report.json")
            report_from_sources(report_path)
            breaks(report_path)
            results, report = evaluate(report_path if from_report else None)
            seen = "error" if report.errors else count(results, None)[0]
        finally:
            TESTING_MD, TESTS_DIR, IMAGE_SRC, IMAGE_MAIN = paths
            shutil.rmtree(work)
        ok = seen == wanted
        failed += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {name}: {seen}, expected {wanted}")
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--missing", action="store_true",
                        help="name every row that has no test")
    parser.add_argument("--computer-results", metavar="JSON",
                        help="the report of pio test -e computer --json-output-path")
    parser.add_argument("--self-test", action="store_true",
                        help="break a copy each way the check guards and require each verdict")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    results, report = evaluate(args.computer_results)

    console(results, args.missing)
    page = os.environ.get("GITHUB_STEP_SUMMARY")
    if page:
        with open(page, "a", encoding="utf-8") as f:
            f.write(summary(results))
            if report.errors:
                f.write("\n### The code and testing.md disagree\n\n")
                f.write("".join(f"- {text}\n" for text in report.errors))

    if report.errors:
        print()
        for text in report.errors:
            print(f"ERROR {text}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
