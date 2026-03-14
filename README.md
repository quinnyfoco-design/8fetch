Hello! 8fetch is a simple info fetching script like neofetch.
This was created by @germanphoneguy and if you have any problems
w/ 8fetch, improvements or distro requests i'd love to be contacted
on tiktok instead of here.


**INSTALLATION**

On Arch systems:

```bash
yay -S 8fetch
```

On other:

```bash
git clone https://github.com/quinnyfoco-design/8fetch.git
```
```
cd 8fetch
```
```
gcc -O2 -o myfetch myfetch.c
```
```
sudo install -Dm755 myfetch /usr/local/bin/8fetch # (if yay on arch doesnt work, this also works for arch.)
```

**RUNNING**

On *all* systems:
```bash
8fetch
```

Flags(newly added):
```bash
--grey # prints the output with no color(default/white).
```
```bash
--color:hexcode # prints the output with the hex color.
```

**UPDATING**

As easy as installing.

On Arch systems:

```bash
yay -S 8fetch
```

On other:

```bash
cd 8fetch
```
```
git pull
```
```
gcc -O2 -o myfetch myfetch.c
```
```
sudo install -Dm755 myfetch /usr/local/bin/8fetch # (if yay on arch doesnt work, this also works for arch.)
```

**KNOWN BUGS**

-layout issues when window to small

-First ascii line being set to the front[FINALLY FIXED]
