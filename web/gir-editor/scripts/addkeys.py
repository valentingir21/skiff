# (rc3) adds i18n keys: python3 scripts/addkeys.py 'key|English|French' ...
import sys, json, re
for lang, idx in (("en", 1), ("fr", 2)):
    p = f"src/lib/i18n/{lang}.ts"; s = open(p).read()
    add = ""
    for a in sys.argv[1:]:
        parts = a.split("|"); k = parts[0]
        if re.search(r'^\s*"' + re.escape(k) + '":', s, re.M): s = re.sub(r'^(\s*)"' + re.escape(k) + r'": .*,$', lambda m: m.group(1) + json.dumps(k) + ": " + json.dumps(parts[idx], ensure_ascii=False) + ",", s, flags=re.M); continue
        add += "  " + json.dumps(k) + ": " + json.dumps(parts[idx], ensure_ascii=False) + ",\n"
    i = s.rstrip().rfind("}")
    s = s[:i].rstrip() + "\n" + add + s[i:]
    open(p, "w").write(s)
