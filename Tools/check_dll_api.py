#!/usr/bin/env python3
"""No member of an exported class is exported again. Found by sitting S1, 2026-09-25.

A module's API macro (VAELEN_RUN_API, VAELENGAME_API, ...) is __declspec(dllexport)
or dllimport only when Unreal builds the module as a DLL - the editor's build. In
every headless build, MSVC's included, it is empty or a visibility attribute, so a
header can put it on a class AND on that class's members and compile everywhere
the CI looks. MSVC refuses it in the editor build:

    Aelvor.h(264,25): error C2487: 'Generations': le membre d'une classe
    d'interface dll ne peut pas etre declare avec une interface dll

Five members of Run::Aelvor carried it from Phase 16 until S1 compiled them. This
reads every header under Source/ the way that build does:

    1  a class or struct declared `class X_API Name ... {` is EXPORTED;
    2  nothing declared directly in an exported class's body carries an *_API
       macro - not a method, not a static, not a friend, not a nested class.

A member of a class that is NOT exported may carry it (CheckpointView::Find
does); a free function may.

    check_dll_api.py [--root DIR]      the check
    check_dll_api.py --self-test       the controls, each shown failing
"""

import argparse
import os
import re
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
API = re.compile(r"[A-Z][A-Z0-9_]*_API$")
TOKEN = re.compile(r"[A-Za-z_]\w*|\{|\}|;|\S")


def strip(text):
    """Comments and string literals blanked, newlines kept so lines still count."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(" " * (min(j + 1, n) - i))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def violations(text):
    """(line, macro, what follows) for every *_API directly in an exported class body."""
    code = strip(text)
    found = []
    scopes = []  # True for an exported class body, False for anything else
    pending = None  # None, or whether the class being declared is exported
    previous = ""
    tokens = [(m.group(0), code.count("\n", 0, m.start()) + 1) for m in TOKEN.finditer(code)]
    for k, (tok, line) in enumerate(tokens):
        if tok in ("class", "struct") and previous != "enum":
            following = tokens[k + 1][0] if k + 1 < len(tokens) else ""
            pending = bool(API.match(following))
        elif API.match(tok) and scopes and scopes[-1]:
            what = " ".join(t for t, _ in tokens[k + 1:k + 4])
            found.append((line, tok, what))
        elif tok == "{":
            scopes.append(bool(pending))
            pending = None
        elif tok == "}":
            if scopes:
                scopes.pop()
        elif tok == ";":
            pending = None
        previous = tok
    return found


def check(root):
    bad = []
    source = os.path.join(root, "Source")
    headers = 0
    for base, dirs, files in os.walk(source):
        dirs.sort()
        for name in sorted(files):
            if not name.endswith((".h", ".hpp", ".inl")):
                continue
            headers += 1
            path = os.path.join(base, name)
            with open(path, encoding="utf-8", errors="replace") as f:
                for line, macro, what in violations(f.read()):
                    bad.append("%s:%d: %s on a member of an exported class (%s ...): C2487 in the editor build"
                               % (os.path.relpath(path, root), line, macro, what))
    return headers, bad


EXPORTED_MEMBER = """
namespace Vaelen::Run
{
	class VAELEN_RUN_API Aelvor
	{
	public:
		/// VAELEN_RUN_API in a comment is not a declaration
		VAELEN_RUN_API uint32 Generations() const noexcept;
		enum class AdoptResult : uint8
		{
			Ok,
		};
	};
}
"""
CLEAN = """
namespace Vaelen::Run
{
	class VAELEN_RUN_API Door final : public Base
	{
	public:
		uint32 Day();
		const char* Name = "VAELEN_RUN_API";
		enum class Why { A, B };
		struct Inner
		{
			int X = 0;
		};
	};
	struct View
	{
		VAELEN_RUN_API const uint8* Find(uint32 Kind) const noexcept;
	};
	VAELEN_RUN_API bool Free(const View& V);
	class VAELEN_RUN_API Forward;
}
"""
MULTILINE = """
class VAELENGAME_API AVaelenThing
	: public AActor
{
	GENERATED_BODY()
public:
	VAELENGAME_API
	static void Spawn();
	void Tick(float DeltaSeconds) override
	{
		if (true) { return; }
	}
	friend VAELENGAME_API void Swap(AVaelenThing& A, AVaelenThing& B);
};
"""


def self_test():
    failures = []

    def expect(label, got, want):
        if got != want:
            failures.append("%s: got %r, want %r" % (label, got, want))

    # Refused: the S1 defect as it stood in Aelvor.h, the comment beside it not counted.
    expect("exported member", [(l, m) for l, m, _ in violations(EXPORTED_MEMBER)], [(8, "VAELEN_RUN_API")])
    # CONTROL: an exported class with no exported member, a nested type, a string that
    # names the macro, a non-exported struct's exported member, a free function and a
    # forward declaration - all allowed.
    expect("clean", violations(CLEAN), [])
    # Refused: the macro on its own line, and on a friend; a function body is not the class's.
    expect("multi-line", [(l, m) for l, m, _ in violations(MULTILINE)], [(7, "VAELENGAME_API"),
                                                                         (13, "VAELENGAME_API")])
    # The check over a tree: the defect is found, and the clean tree passes.
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "Source", "VaelenRun", "Public"))
        path = os.path.join(tmp, "Source", "VaelenRun", "Public", "Aelvor.h")
        with open(path, "w", encoding="utf-8") as f:
            f.write(EXPORTED_MEMBER)
        headers, bad = check(tmp)
        expect("tree with the defect", (headers, len(bad)), (1, 1))
        with open(path, "w", encoding="utf-8") as f:
            f.write(CLEAN)
        expect("tree without it", check(tmp), (1, []))
    for f in failures:
        print("SELF-TEST FAILED: " + f)
    print("check_dll_api self-test: %s" % ("FAILED" if failures else "4 controls passed"))
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=ROOT)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    headers, bad = check(args.root)
    for b in bad:
        print(b)
    print("check_dll_api: %d headers, %d exported members of exported classes" % (headers, len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
