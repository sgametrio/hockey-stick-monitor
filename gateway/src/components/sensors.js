const ACC_FULLSCALE = 4.0  // g
const GYR_FULLSCALE = 2000.0  // dps
const MAG_FULLSCALE = 4.0  // gauss
const MAX = (2**(16-1))  // 32768

export const mapping = [
   "acc_top",
   "gyr_top",
   "mag_top",
   "acc_mid",
   "gyr_mid",
   "mag_mid",
   "acc_bot",
   "gyr_bot",
   "mag_bot"
]

const mapping_arrays = {}
for (let value of mapping) {
   mapping_arrays[value] = []
}

export const convert_raw = {
   "acc_top": (int16_t) => int16_t * ACC_FULLSCALE / MAX,
   "gyr_top": (int16_t) => int16_t * GYR_FULLSCALE / MAX,
   "mag_top": (int16_t) => int16_t * MAG_FULLSCALE / MAX,
   "acc_mid": (int16_t) => int16_t * ACC_FULLSCALE / MAX,
   "gyr_mid": (int16_t) => int16_t * GYR_FULLSCALE / MAX,
   "mag_mid": (int16_t) => int16_t * MAG_FULLSCALE / MAX,
   "acc_bot": (int16_t) => int16_t * ACC_FULLSCALE / MAX,
   "gyr_bot": (int16_t) => int16_t * GYR_FULLSCALE / MAX,
   "mag_bot": (int16_t) => int16_t * MAG_FULLSCALE / MAX,
}
export const sensors = mapping_arrays