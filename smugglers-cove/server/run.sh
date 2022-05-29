#!/bin/bash

chmod 777 /launchersock/launchersock

supervisord -c /supervisord.conf

