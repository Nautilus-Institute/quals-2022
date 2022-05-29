# sameold

A mic check service.
Test if the team's submitted string (1) starts with the punycode of their name, and (2) CRC32s to the same CRC32 checksum of the word "the" (without quotes).

Allowed characters: `string.ascii_letters + string.digits`

## Description on the scoreboard

```
Hack ___ planet!

Submit a string that complies with the following rules:

- The string should start with the punycode of your team name. This is a good time for you to figure out with which team you are playing.
- After your team name, you may add any number of alphanumeric characters.
- `CRC32(the_intended_answer) == CRC32(your_string)`
```

## Potential answers (without a team name)

`d9r0Ou`
`nBLpwH`
`B3HZDV`
`PoD7mL`
`QsJZwA`
`XeRTad`
`ZYWJ8y`
`3BUSbG`
`ajUuZEX`
`alL3Utj`
`amLrdos`
`aopwz6n`
`aUdspEQ`

## Potential answers (with a team name)

`Nautilus Instituteb2A7HT`
