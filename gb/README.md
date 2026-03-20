# gbtty - GB client

This is the GB client application which actually runs on the Game Boy and does
the emulating of the VT100 by interpretting input delivered over the link port.

It's primarily a consumer, but does support some limited responses back to the
host, including cursor reporting, device attributes, as well as ACK and SYN IDLE.

## License

As mentioned in the [top-level README](../README.md) for the project, the GB client is licensed
under the terms of the GNU General Public License Version 3.

It uses elements of other projects licensed under the GNU General Public License
Version 3 or compatible licenses, including:

- [Better Button Test](https://github.com/orangeglo/better-button-test) by orangelo - original foundation of `src/main.c`, `src/palettes.h`
- [Portfolio 6x8](https://int10h.org/oldschool-pc-fonts/fontlist/font?portfolio_6x8) - bitmap font, based on FON version from [VileR](https://int10h.org/oldschool-pc-fonts/). Originally licensed under CC-BY-SA 4.0. I converted it to 8x8 1bpp tiles, which are a derivative work licensed under GNU General Public License Version 3.
