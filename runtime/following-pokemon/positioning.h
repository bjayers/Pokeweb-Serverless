#ifndef POKEWEB_FOLLOWING_POSITIONING_H
#define POKEWEB_FOLLOWING_POSITIONING_H
#include <stdint.h>
/* Read only the chosen record from member 0 of the ROM NARC. Failure leaves
 * the caller's calculated legacy positioning in place. */
int fwp_land(unsigned row, unsigned count, uint32_t registry_crc, uint8_t out[12]);
#ifdef FW_MOUNT
int fwp_surf(unsigned row, unsigned count, uint8_t out[8]);
#endif
/* Per-zone follower overrides, member 1 of the same NARC when an editor has
 * appended it. Flags bit 0 swaps the large-follower rule for that zone:
 * shown in an interior, recalled in an exterior. */
#define FWP_ZONE_MAX        256u
#define FWP_ZONE_LARGE_SWAP 1u
void fwp_zones_load(void);
unsigned fwp_zone_flags(unsigned zone);
#endif
