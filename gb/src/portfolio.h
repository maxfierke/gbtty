#ifndef portfolio_H
#define portfolio_H

#include <gbdk/platform.h>
#include <stdint.h>

#define portfolio_TILE_ORIGIN 0
#define portfolio_TILE_W 8
#define portfolio_TILE_H 8
#define portfolio_WIDTH 256
#define portfolio_HEIGHT 64
#define portfolio_TILE_COUNT 256
#define portfolio_MAP_ATTRIBUTES 0
extern const unsigned char portfolio_map[256];
#define portfolio_map_attributes portfolio_map

BANKREF_EXTERN(portfolio)

extern const uint8_t portfolio_tiles[2048];

#endif
