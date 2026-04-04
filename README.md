Hello! 8fetch is a simple info fetching script like neofetch.
This was created by @germanphoneguy and if you have any problems
w/ 8fetch, improvements or distro requests i'd love to be contacted
on tiktok instead of here.


**INSTALLATION**

On Arch systems:

```bash
yay -S 8fetch
```

On other(if yay on arch doesnt work, this also works for arch.):

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
sudo install -Dm755 myfetch /usr/local/bin/8fetch
```

+----------------+

If you are on a system without root(School systems or unrooted android devices):

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
mkdir -p ~/bin
```
```
install -Dm755 myfetch ~/bin/8fetch
```

Please check what shell u are using(with 'echo $SHELL'):

Bash: 
```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc
```
Zsh:
```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
```
Fish:
```bash
fish_add_path ~/bin
```
I only know the syntax of these 3, if you use another shell please lookup of what the equivalent to these are of ur shell.

+----------------+

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

On other(if yay on arch doesnt work, this also works for arch.):

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
sudo install -Dm755 myfetch /usr/local/bin/8fetch
```

**KNOWN BUGS**

-layout issues when window to small

-First ascii line being set to the front[FINALLY FIXED]
