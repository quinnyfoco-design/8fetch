Hello! 8fetch is a simple info fetching script like neofetch.
This was created by @germanphoneguy and if you have any problems
w/ 8fetch or improvements id love to be contacted on tiktok.

If you wanna compile from source or read the code, check myfetch.c.
The full code and how to compile it is there.

**INSTALLATION**

On Arch systems:

```bash
git clone https://github.com/quinnyfoco-design/8fetch.git
```
```
cd 8fetch
```
```
makepkg -si
```

On other:

```bash
git clone https://github.com/quinnyfoco-design/8fetch.git
```
```
cd 8fetch
```
```
sudo install -Dm755 myfetch /usr/local/bin/8fetch
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
cd 8fetch
```
```
git pull
```
```
makepkg -si
```

On other:

```bash
cd 8fetch
```
```
git pull
```
```
sudo install -Dm755 myfetch /usr/local/bin/8fetch (if makepkg on arch doesnt work, this also works for arch.)
```

**KNOWN BUGS**

-layout issues when window to small

-First ascii line being set to the front[FINALLY FIXED]
