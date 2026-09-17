# shout
shout is a minimal audio backend for POSIX operating systems that works using pipes

## why
imagine you are making something that requires audio and you want to port it to multiple platforms... probably adding support for each one is gonna be a hell so this is why shout exists

also for easy audio for pipes

## install
clone the repo, enter to the directory and run:
```
chmod +x COMPILE
./COMPILE <backend>
```
where backend is the backend you want to, for example, if you want pipewire, you can do:
```
./COMPILE pipewire
```

then move the binary whenever you want, does not matter if is ~/.local/bin or /bin or whatever, just have it on your path
## available backends.
the only available backends are:
- Pipewire

I will work on more, but if you want to implement a backend for the platform you want, please do a pr

## usage
<command who gives PCM> | shout

flags:
- -s <path> : where do you want your socket, /tmp/shout by default
- -r <rate> : PCM Hz, 48000 by default
- -b 16|32 : bit-depth, 16 by default
- -c 1|2, 2 by default
- -d : turn log, disabled by default

example of how to send data to shout:
```
ffmpeg -i myfavoritesong.flac -f s16le -ar 48000 -ac 2 - | shout -r 48000 -b 16 -c 2
```

## license
uh all the code is under public domain except from sout, which is MIT
