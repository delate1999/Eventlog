# Eventlog

Simple eventlog module to be used on embedded devices. Example of usage is located in main.c file at the root of the repository.

Event have simple structure: ID & time. Additionally coutner is available to check if all events were written successfully.  

## Overview

- Provide callbacks to write, read, erase to & from memory (ex. ext flash).
- Define how much memory will be devoted to storing events.

##


## Features
- Lightweight and easy to integrate
- Flash wear leveling
- Small event size
- Circular buffer - oldest events are overwritten with new ones
- Header page contains last written to offset - fast bring up time, even with huge event memory

## License

MIT

