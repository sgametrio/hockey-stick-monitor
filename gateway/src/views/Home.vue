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
                <span class="text-xs mx-1">{{valuesFromArduinos[i][valuesFromArduinos[i].length - 1]}}</span>
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
        <!-- <input class="text-blue-700 text-md bg-blue-100 shadow px-3 py-2 border-blue-200 border-2 rounded-lg m-3
                      hover:bg-blue-200 hover:shadow-md" 
               v-if="!(arduino && characteristic) && !loading" type="button" @click="connectToArduino" value="Connect to Arduino"/>
        <template v-else>
          <div class="mt-5">
            <loading-spinner class="p-3" v-if="loading"/>
            <div v-else @click="changeRecordingState">
              <color-svg v-if="!recording" class="border-2 border-purple-200 text-purple-500 bg-purple-100 hover:bg-purple-200 p-3 pl-4 rounded-full h-20 w-20 shadow" stroke=2 fill=true icon="play"/>
              <color-svg v-else class="border-2 border-yellow-200 text-yellow-500 bg-yellow-100 hover:bg-yellow-200 p-3 rounded-full h-20 w-20 shadow" stroke=1 fill=true icon="pause"/>
            </div>
          </div>
          <div class="max-w-full min-w-full h-24">
            <div class="flex flex-wrap">
              <div class="p-1" v-for="(val, i) in readValues" :key="i">
                {{ val }}&nbsp;
              </div>
            </div>
          </div>
        </template> -->
      </div>
    </div>    
  </div>
</template>

<script>
import { log } from "@/components/console"
import ColorSvg from "@/components/ColorSvg"
import LoadingSpinner from "@/components/LoadingSpinner"

export default {
  name: 'home',
  components: {
    "color-svg": ColorSvg,
    "loading-spinner": LoadingSpinner,
  },
  data: function () {
    return {
      arduinos: [],
      serviceUuid: "0xabc0",
      characteristicUuid: "0xabc1",
      characteristics: [],
      valuesFromArduinos: [],
      recordings: [],
      loading: false,
    }
  },
  computed: {},
  mounted() {},
  methods: {
    async connectToArduino() {
      this.loading = true
      const serviceUuidInt = parseInt(this.serviceUuid)
      const characteristicUuidInt = parseInt(this.characteristicUuid)

      try {
        log('Requesting Bluetooth Device...')
        const arduino = await navigator.bluetooth.requestDevice({
          filters: [{services: [serviceUuidInt]}]
        })
      
        log('Connecting to GATT Server...')
        const server = await arduino.gatt.connect();

        log('Getting Service...')
        const services = await server.getPrimaryServices()
        console.log(services)
        const service = await server.getPrimaryService(serviceUuidInt)

        log('Getting Characteristics...')
        const characteristic = await service.getCharacteristic(characteristicUuidInt)

        this.characteristics.push(characteristic)
        this.arduinos.push(arduino)
        this.recordings.push(false)
        this.valuesFromArduinos.push([])
      } catch (error) {
        log("Error connecting to arduino: " + error)
      } finally {
        this.loading = false
      }
    },
    async startRecording(i) {
      const characteristic = this.characteristics[i]
      try {
        await characteristic.startNotifications()
        characteristic.addEventListener('characteristicvaluechanged', (event) => this.appendReadValue(event, i))
        // To send data
        // let encoder = new TextEncoder('utf-8');
        // characteristic.writeValue(encoder.encode("a"))
        this.recordings.splice(i, 1, true)
      } catch(error) {
        log('Error listening to characteristic ' + error);
      }
    },
    async stopRecording(i) {
      const characteristic = this.characteristics[i]
      try {
        await characteristic.stopNotifications()
        characteristic.removeEventListener('characteristicvaluechanged', (event) => this.appendReadValue(event, i))
        this.recordings.splice(i, 1, false)
      } catch(error) {
        log('Error listening to characteristic ' + error);
      }
    },
    async changeRecordingState(i) {
      const characteristic = this.characteristics[i]
      const recording = this.recordings[i]
      this.loading = true
      if (recording) {
        characteristic.stopNotifications()
        characteristic.removeEventListener('characteristicvaluechanged', (event) => this.appendReadValue(event, i))
      } else {
        try {
          await characteristic.startNotifications()
          characteristic.addEventListener('characteristicvaluechanged', (event) => this.appendReadValue(event, i))
          // To send data
          let encoder = new TextEncoder('utf-8');
          characteristic.writeValue(encoder.encode("a"))
        } catch(error) {
          log('Error listening to characteristic ' + error);
        }
      }

      this.recordings[i] = !this.recordings[i]
      this.loading = false
    },
    appendReadValue (event, i) {
      this.valuesFromArduinos[i].push(event.target.value.getUint8(0))
    }
  }
}
/*
  {
    "arduino": "uuid",
    "accelerometer_bottom": [[x, y, z], ...],
    "accelerometer_top": [[x, y, z], ...],
    "gyroscope_bottom": [[x, y, z], ...],
    "gyroscope_top": [[x, y, z], ...],
    "measurements_gap": milliseconds
  }

 */
</script>

