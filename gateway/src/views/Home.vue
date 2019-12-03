<template>
  <div class="flex items-center justify-center w-full h-full bg-gray-200">
    <div class="flex flex-col
                h-112 w-84 sm:h-128 sm:w-96 md:h-160 md:w-140 lg:h-160 lg:w-192 p-5 
                border rounded-lg shadow-md bg-white">
      <div 
        class="self-end text-sm rounded-full px-3 py-1"
        :class="[ (arduinos.length > 0) ? 'text-indigo-500 bg-indigo-100' : 'text-red-700 bg-red-200']"
      >
        <color-svg class="h-4 w-4 inline mr-1" stroke=2 :icon="(arduinos.length > 0) ? 'wifi' : 'wifi-off'"/>
        {{ (arduinos.length > 0) ? `Connected to ${arduinos.length} device${(arduinos.length > 1) ? "s" : ""}` : "Not connected to devices" }}
      </div>
      <div class="w-full h-full flex flex-col items-center justify-center">
        <div>
          <h1 class="font-bold text-3xl">SCHAM</h1>
          <h2>a Super Cool Hockey stick Adviser and Monitor</h2>
        </div>
        <div class="flex flex-wrap items-start mt-5">
          <div v-for="(arduino, i) in arduinos" :key="i" class="mr-2">
            <div class="h-52 w-32 shadow rounded-lg flex flex-col items-center justify-center">
              <img src="img/icons/arduino-nano.svg" class="h-20 w-20 bg-gray-200 rounded-full shadow border-2 border-gray-200"/>
              <span class="mt-4 text-xs break-words w-24">{{ arduino.name }}</span>
              <div class="mt-2 flex flex-row justify-between items-center w-24">
                <color-svg :class="[ (recordings[i]) ? 'border-gray-100 text-gray-500 bg-gray-100' : 'border-teal-200 text-teal-500 bg-teal-100 hover:bg-teal-200']"
                           class="border p-1 pl-2 rounded-full h-8 w-8 shadow" stroke=2 fill=true 
                           icon="play" @click.native="startRecording(i)"/>
                <span class="text-xs mx-1" v-if="showCounter">{{counter}}</span>
                <color-svg :class="[ (!recordings[i]) ? 'border-gray-100 text-gray-500 bg-gray-100' : 'border-yellow-200 text-yellow-500 bg-yellow-100 hover:bg-yellow-200']"
                           class="border p-1 rounded-full h-8 w-8 shadow" stroke=1 fill=true 
                           icon="pause" @click.native="stopRecording(i)"/>
              </div>
            </div>
          </div>
          <div>
            <div class="h-52 w-32 shadow rounded-lg flex flex-col items-center justify-center">
              <color-svg v-if="!loading" @click.native="connectToArduino" class="h-16 w-16 text-purple-500 bg-purple-100 hover:bg-purple-200 p-2 rounded-full shadow" stroke=2 icon="plus"/>
              <loading-spinner v-else/>
              <span class="text-sm mt-5 w-24">Connect to a new device</span>
            </div>
          </div>
        </div>
      </div>
    </div>    
  </div>
</template>

<script>
import { log } from "@/components/console"
import ColorSvg from "@/components/ColorSvg"
import LoadingSpinner from "@/components/LoadingSpinner"
import { mapping } from "@/components/sensors"
import Api from "@/components/Api"

export default {
  name: 'home',
  components: {
    "color-svg": ColorSvg,
    "loading-spinner": LoadingSpinner,
  },
  data: function () {
    return {
      PACKET_BURST: 100,
      arduinos: [],
      serviceUuid: "6a0a5c16-0dcf-11ea-8d71-362b9e155667",
      managementUuid: "5143d572-0dcf-11ea-8d71-362b9e155667",
      valuesUuid: "2541489a-0dd1-11ea-8d71-362b9e155667",
      ackUuid: "165c644c-c0b4-43e7-baf1-5b13dfd55732",
      managements: [],
      valuesCharacteristics: [],
      acks: [],
      values: [],
      recordings: [],
      loading: false,
      counter: 0,
      showCounter: false
    }
  },
  // computed: {
  //   valuesLength: function () {
  //     return this.values.length
  //   }
  // },
  // watch: {
  //   valuesLength: async function (after, before) {
  //     if (after % PACKET_BURST === 0) {
  //       // TODO: be sure that we do not lose some values
  //       console.log("wee")
  //       const dump = [...this.values]
  //       this.values = []
  //       for (let [values, i] of dump) {
  //         if (values.length > 0) {
  //           await Api.sendData({
  //             ...dump
  //           }, this.arduinos[i].id)
  //         }
  //       }
  //     }
  //   }
  // },
  mounted() {},
  beforeDestroy() {
    // ensure that I remove event listeners on characteristics
    for (let [characteristic, i] of this.values) {
      characteristic.removeEventListener('characteristicvaluechanged', (event) => this.newValueToRead(event, i), { passive: true })
    }
  },
  methods: {
    async connectToArduino() {
      this.loading = true

      try {
        log('Requesting Bluetooth Device...')
        const arduino = await navigator.bluetooth.requestDevice({
          filters: [{
            services: ["6a0a5c16-0dcf-11ea-8d71-362b9e155667"]
          }]
        })
      
        log('Connecting to GATT Server...')
        const server = await arduino.gatt.connect();

        log('Getting Service...')
        const service = await server.getPrimaryService(this.serviceUuid)

        log('Getting managements...')
        const management = await service.getCharacteristic(this.managementUuid)
        const valuesCharacteristic = await service.getCharacteristic(this.valuesUuid)
        // const ack = await service.getCharacteristic(this.ackUuid)

        this.managements.push(management)
        this.valuesCharacteristics.push(valuesCharacteristic)
        // this.acks.push(ack)
        this.arduinos.push(arduino)
        this.recordings.push(false)
        const sensors = {}
        for (let value of mapping) {
          if (value != null)
            sensors[value] = []
        }
        this.values.push(sensors)
      } catch (error) {
        log("Error connecting to arduino: " + error)
      } finally {
        this.loading = false
      }
    },
    async startRecording(i) {
      const values = this.valuesCharacteristics[i]
      const management = this.managements[i]
      // const ack = this.acks[i]
      try {
        // await ack.startNotifications()
        await values.startNotifications()
        values.addEventListener('characteristicvaluechanged', async (event) => {
          await this.readValues(event, i)
        }, false)
        // communicate to start reading data
        const start = Uint8Array.from([0x01])
        this.counter = 0
        this.showCounter = false
        await management.writeValue(start)
        this.recordings.splice(i, 1, true)
      } catch(error) {
        log('Error listening to characteristic ' + error);
      }
    },
    async stopRecording(i) {
      const values = this.valuesCharacteristics[i]
      const management = this.managements[i]
      // const ack = this.acks[i]
      try {
        // communicate to stop reading data
        const stop = Uint8Array.from([0x00])
        await management.writeValue(stop)
        // await values.stopNotifications()
        // values.removeEventListener('characteristicvaluechanged', async (event) => {
        //     await this.newValueToRead(event, i)
        // }, false)
        this.showCounter = true
        console.log(this.values)
        this.recordings.splice(i, 1, false)
      } catch(error) {
        log('Error listening to characteristic ' + error)
      }
    },
    // async newValueToRead(event, i) {
    //   const byte = event.target.value.getUint8(0)
    //   if (byte === 1) {
    //     const read = Uint8Array.from([0x02])
    //     await this.readValues(i)
    //     // await this.acks[i].writeValue(read)
    //   }
    // },
    async readValues(event, i) {
      this.counter++
      console.log(this.counter)
      // a single message is 3*4*2*3 = 72 byte (x,y,z)*(4 byte)*(2 sensor)*(3 IMU)
      // messages contains N message + 1 byte of packet_id (at the end)
      const packet = event.target.value
      const messages = packet.buffer.slice(0, packet.byteLength - 1)
      const packet_id = packet.getUint8(packet.byteLength - 1)
      // alert(messages.byteLength)
      for (let m = 0; m < messages.byteLength; m += 72) {  // for every message
        for (let s = 0; s < (72 / 12); s++) {  // for every sensor read
          const xyz = new Float32Array(messages.slice(m + s*12, m + s*12 + 12), 0, 3)
          this.values[i][mapping[s]].push(xyz)
        }
      }
    }
  }
}
</script>

