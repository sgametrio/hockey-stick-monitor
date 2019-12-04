export const mapping = [
   "acc_top",
   "gyr_top",
   "acc_mid",
   "gyr_mid",
   "acc_bot",
   "gyr_bot"
]

const mapping_arrays = {}
for (let value of mapping) {
   mapping_arrays[value] = []
}

export const sensors = mapping_arrays