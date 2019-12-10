<template>
  <div class="flex flex-col items-center justify-center w-full h-full bg-gray-200">
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
        <div>
          <div id="canvas"></div>
          <input type="button" value="Start render" @click="renderStick"/>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { log } from "@/components/console"
import ColorSvg from "@/components/ColorSvg"
import LoadingSpinner from "@/components/LoadingSpinner"
import { mapping, sensors, convert_raw } from "@/components/sensors"
import Api from "@/components/Api"
import RenderStick from "@/components/RenderStick"

export default {
  name: 'home',
  components: {
    "color-svg": ColorSvg,
    "loading-spinner": LoadingSpinner
  },
  data: function () {
    return {
      N_MEASUREMENTS: 20,
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
  computed: {
    valuesFirst () {
      if (this.values.length === 0)
        return 0

      return this.values[0][mapping[0]].length
    }
  },
  watch: {
    async valuesFirst (after, before) {
      if (after > 0 && after % this.N_MEASUREMENTS === 0) {
        await this.sendSensorsData()
      }
    }
  },
  mounted() {},
  beforeDestroy() {
    // ensure that I remove event listeners on characteristics
    for (let [characteristic, i] of this.valuesCharacteristics) {
      characteristic.removeEventListener('characteristicvaluechanged', async (event) => {
        await this.readValues(event, i)
      }, false)
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
          // acceptAllDevices: true,
          // optionalServices: ["6a0a5c16-0dcf-11ea-8d71-362b9e155667"]
        })
      
        log('Connecting to GATT Server...')
        const server = await arduino.gatt.connect();

        log('Getting Service...')
        const service = await server.getPrimaryService(this.serviceUuid)

        log('Getting managements...')
        const management = await service.getCharacteristic(this.managementUuid)
        const valuesCharacteristic = await service.getCharacteristic(this.valuesUuid)

        this.managements.push(management)
        this.valuesCharacteristics.push(valuesCharacteristic)
        this.arduinos.push(arduino)
        this.recordings.push(false)
        this.values.push(JSON.parse(JSON.stringify(sensors)))
      } catch (error) {
        log("Error connecting to arduino: " + error)
      } finally {
        this.loading = false
      }
    },
    async startRecording(i) {
      const values = this.valuesCharacteristics[i]
      const management = this.managements[i]
      try {
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
        await Api.startCommunication({
          "mgap": 10,
          "time": (new Date()).getTime()  
        }, this.arduinos[i].name)
      } catch(error) {
        log('Error listening to characteristic ' + error);
      }
    },
    async stopRecording(i) {
      const management = this.managements[i]
      try {
        // communicate to stop reading data
        const stop = Uint8Array.from([0x00])
        await management.writeValue(stop)
        console.log("pausa")

        this.showCounter = true
        this.recordings.splice(i, 1, false)
      } catch(error) {
        log('Error listening to characteristic ' + error)
      }
    },
    async sendEndingSequence(i) {
      const values = this.valuesCharacteristics[i]
      await values.stopNotifications()
      values.removeEventListener('characteristicvaluechanged', async (event) => {
          await this.newValueToRead(event, i)
      }, false)
      // TODO: investigate why it's not sequential
      if (this.valuesFirst > 0) {
        const weeeeee = await this.sendSensorsData(/* i */)
      }
      const stopped = await Api.stopCommunication({}, this.arduinos[i].name)
    },
    async readValues(event, i) {
      this.counter++
      // console.log(this.counter)
      // a single message is 3*2*2*3 = 36 byte (x,y,z)*(2 byte)*(2 sensor)*(3 IMU)
      // messages contains N message + 1 byte of packet_id (at the end)
      const packet = event.target.value
      const messages = packet.buffer.slice(0, packet.byteLength - 1)
      const packet_id = packet.getUint8(packet.byteLength - 1)
      console.log(packet)
      // Check if it's last packet
      if (packet.getUint8(0) === 255) {
        // Send remaining data and then send to server the stop sequence
        await this.sendEndingSequence(i)
        console.log("disconnesso")
        return
      }

      for (let m = 0; m < messages.byteLength; m += 36) {  // for every message
        for (let s = 0; s < (36 / 6); s++) {  // for every sensor read
          const xyz = new Int16Array(messages.slice(m + s*6, m + s*6 + 6), 0, 3)
          // console.log("Received buffer: ", xyz.buffer)
          let computed = new Float32Array(3)
          let read = new Int16Array(3)
          for (let x = 0; x < 3; x++) {  // convert from int16 to float according to fullscale
            computed[x] = convert_raw[mapping[s]](xyz[x])
          }
          this.values[i][mapping[s]].push([...computed.values()])
        }
      }
    },
    sendSensorsData (/* i */) {
      // TODO: be sure that we do not lose some values in doing this dump stuff
      const dump = JSON.parse(JSON.stringify(this.values[0]))
      // To avoid losing reactivity
      this.$set(this.values, 0, JSON.parse(JSON.stringify(sensors)))
      if (dump["acc_top"].length === 0) {
        return
      }
      return Api.sendData({
        ...dump
      }, this.arduinos[0].name)
    },
    renderStick () {
      this.renderer = RenderStick.getRenderer()
      this.$el.querySelector("#canvas").appendChild(this.renderer.domElement)
      this.animate()
    },
    animate() {
      requestAnimationFrame(this.animate)
      RenderStick.rotateAndRender({x: 0, y: 0, z: 0})
   }
  }
}
</script>

