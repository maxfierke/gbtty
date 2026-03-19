# gbtty - GB client

This is the GB client application which actually runs on the Game Boy and does
the emulating of the VT100 by interpretting input delivered over the link port.

It's primarily a consumer, but does support some limited responses back to the
host, including cursor reporting, device attributes, as well as ACK and SYN IDLE.

## License

As mentioned in the [top-level README](../README.md) for the project, the GB client is licensed
under the terms of the GNU General Public License Version 3.

It uses elements of other projects licensed under the GNU General Public License
Version-3, including:

- [Better Button Test](https://github.com/orangeglo/better-button-test) by orangelo - original foundation of `src/main.c`, `src/palettes.h`
- [oldschool-cga-8x8](https://github.com/susam/pcface) by susam - [`res/oldschool-cga-8x8.png`](res/oldschool-cga-8x8.png)
