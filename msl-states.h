#ifndef MSL_STATES_H
#define MSL_STATES_H

#include <stdint.h>
#include "msl-timings.h"
extern const struct msl_timing_chart state_80[];
extern const struct msl_timing_chart state_E2_E3[];
extern const struct msl_timing_chart state_E0[];
extern const struct msl_timing_chart state_E4[];
extern const struct msl_timing_chart state_E6[];
extern const struct msl_timing_chart state_E5[];
extern const struct msl_timing_chart state_E7[];

uint8_t state_E2_E3_TO80_CI89(struct ge *ge);
uint8_t state_E2_E3_TI06_CI82(struct ge *ge);
uint8_t state_E2_E3_TI06_CU04(struct ge *ge);
uint8_t state_80_TI06_CU01(struct ge *ge);
uint8_t state_80_TI06_CU03(struct ge *ge);
uint8_t state_80_TI06_CU05(struct ge *ge);
uint8_t state_E0_TO70_CI60(struct ge *ge);
uint8_t state_E0_TI06_CU17(struct ge *ge);
uint8_t state_E6_TI06_CU03(struct ge *ge);
uint8_t state_E6_TI06_CU17(struct ge *ge);
uint8_t state_E6_TO80_CI38(struct ge *ge);
uint8_t state_E5_TI06_CU17(struct ge *ge);
uint8_t state_E5_TO70_CI60(struct ge *ge);
uint8_t state_E5_TO80_CI31(struct ge *ge);
uint8_t state_E7_TO80_CI38(struct ge *ge);
uint8_t state_E7_TI06_CU17(struct ge *ge);


#endif /* MSL_STATES_H */
