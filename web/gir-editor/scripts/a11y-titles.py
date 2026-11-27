# (rc3) an icon-only <button title={x}>✎</button> gets aria-label={x} too (a title is no accessible name for the lint)
import re, sys, glob
def fix(path):
    s=open(path).read(); out=[]; i=0
    while True:
        j=s.find('title={',i)
        if j<0: out.append(s[i:]); break
        k=j+7; d=1
        while d:
            c=s[k]; d+= c=='{'; d-= c=='}'; k+=1
        expr=s[j+7:k-1]; ts=s.rfind('<',0,j); te=s.find('>',k); tag=s[ts:te]; nxt=s[te+1:te+40]
        out.append(s[i:k])
        if tag.startswith('<button') and 'aria-label' not in tag and re.match(r'\s*[^\w\s<{]{1,3}\s*</button>', nxt): out.append(' aria-label={'+expr+'}')
        i=k
    n=''.join(out)
    if n!=s: open(path,'w').write(n); print(path)
for p in glob.glob('src/**/*.svelte', recursive=True): fix(p)
