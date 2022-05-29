# Teedium Wallet

OPTEE Bitcoin Wallet Trusted Application Pwnable:

```
Crypto coins in your Trusted Zone?

It's more likely than you think.
```

### Run

This script will run the challenge in a docker container

```
tar xvzf teedium_wallet_dist.tar.gz
cd secure-world-wallet
./docker_run.sh
```

### Build

Clone OPTEE-Build, build that, and

```
cd challenge
TA_DEV_KIT_DIR=<path-to-optee_os>/out/arm/export-ta_arm32> CROSS_COMPILE=<path-to-toolchains>/aarch32/bin/arm-linux-gnueabihf- make --no-builtin-variables
```