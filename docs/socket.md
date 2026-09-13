# Default socket
The default socket file is
```
/tmp/shout
```
If no socket path is specified, this is the one that is used.

# Custom Path
Because shout **Can only handle one client**, for having multiple programs the "solution" was allow a program to create his own shout instance with
```
shout -s [path]
```
shout uses minimal resources, so having multiple shout's its not a big deal
