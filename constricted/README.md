# Constricted

Rust Javascript Pwnable:

```
Everyone keeps telling me that once we re-write javascript engines in rust, they will be safe! In that case this will be some of the safest javascript you will ever write
```

### Run

This script will run the challenge in a docker container with the given input file

```
cd challenge
./run.sh ./path/to/file.js
```

### Build

This script will clone and build the Boa crate after applying the challenge patch

```
cd challenge
./build_local_copy.sh
```
