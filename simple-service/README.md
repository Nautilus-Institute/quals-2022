# Mic Check: Hosted Service #

**Category:** Mic Check
**Author:** @[fuzyll](https://fuzyll.com/)

Some challenges, like this one, will require you to connect to a remote
server and interact with it. Connect to <HOST> on port 31337, receive the
prompt from the service, send back the required input, and then receive the
response. If you provided the right input, the flag will be in the response.

Note that these challenges usually have a server-side timeout on connections.
This one is pretty forgiving, but others won't be. We highly recommend
automating your interactions with this challenge because future ones will
require it.


## Building ##

This repository contains thtree Docker images: The `challenge`, the `solver`,
and a `test` image capable of hosting a single instance of the `challenge`.
You can build the first two with:

```
make build
```

The resulting Docker images will be tagged as `simple-service-challenge:latest` and
`simple-service-solver:latest`.

You can also build just one of them with `make challenge` or `make solver`
respectively.

See below for building and using the `test` image.


## Deploying ##

**Static Files:**: None

There is a single service for this challenge. See `simple-service.xinetd' for
more details.


## Testing ##

Use `make test` to test locally. This will build the `simple-service-test`
image using the latest version of the `challenge` image as a base, then set
up a Docker network to host and throw against the running service.


## Notes ##

This challenge is intended to be part of a tutorial/infrastructure check
series that we can use to ensure new players know how to play the game and
old players have their environment functioning properly. This is also a good
starting point for our infrastructure development to ensure we are capable of
hosting a service for all teams.
