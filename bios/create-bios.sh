#!/bin/bash
cd seabios
make
cd ..
python parse-fonts.py
python palette.py
python alter-bios.py
