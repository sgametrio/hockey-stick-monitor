import * as ThreeJS from 'three'
import * as OBJLoader from 'three-obj-loader'
const l2norm = require("compute-l2norm")

OBJLoader(ThreeJS)

function latest(array) {
   if (array.length === 0)
      return 0
   return array[array.length - 1]
}

class RenderStick {
   constructor({width = 400, height = 400}) {
      this.ThreeJS = ThreeJS
      this.scene = new this.ThreeJS.Scene()
      this.camera = new this.ThreeJS.PerspectiveCamera(110, width / height, 0.1, 10000)
      // TODO: resize canvas on resize window
      // TODO: add material to stick
      this.renderer = new this.ThreeJS.WebGLRenderer({ alpha: true, antialias: true })
      this.ambientLight = new this.ThreeJS.HemisphereLight(0x444444, 0xaaaaaa, 1)
      this.shadowLight = new this.ThreeJS.DirectionalLight(0xff0000, 0.8)
      this.shadowLight.castShadow = true
      this.shadowLight.position.set(0, 1000, 0)
      this.shadowLight.shadow.mapSize.width = 2048
      this.shadowLight.shadow.mapSize.height = 2048
      this.shadowLight.shadow.camera.far = 3500

      this.shadowLightHelper = new this.ThreeJS.DirectionalLightHelper(this.shadowLight, 500)
      

      var geometry = new this.ThreeJS.PlaneGeometry(10000, 10000)
      var material = new this.ThreeJS.MeshBasicMaterial( {color: 0xeeeeee, side: this.ThreeJS.DoubleSide} )
      this.plane = new this.ThreeJS.Mesh( geometry, material );
      this.plane.rotation.x = Math.PI / 2
      this.plane.position.y = -1200
      this.plane.receiveShadow = true
      this.scene.add( this.plane );
      // this.stickLight = new this.ThreeJS.DirectionalLight(0xffffff, 1)
      this.scene.add(this.ambientLight)
      // this.scene.add(this.shadowLight)
      // this.scene.add(this.shadowLightHelper)
      this.renderer.setSize(width, height)
      this.renderer.setClearColor(0x000000)
      this.renderer.shadowMap.enabled = true
      this.renderer.shadowMap.renderReverseSided = false
      this.stick = null
      
      let loader = new this.ThreeJS.OBJLoader()
      loader.load("model/hockey_stick.obj", (stick) => {
         this.stick = stick
         this.axesHelper = new this.ThreeJS.AxesHelper(1000);
         this.stick.position.set(0, 0, 700)
         this.stickPivot = new this.ThreeJS.Object3D()
         this.stickPivot.position.set(0, 0, 0)
         this.stickPivot.add(this.stick)
         this.stickPivot.add(this.axesHelper)
         // this.stick.rotation.z = Math.PI / 2
         this.camera.position.set(200, 1100, 400)
         // this.camera.rotateOnAxis(new this.ThreeJS.Vector3(1,0,0), Math.PI / 2)
         // this.camera.rotateX(Math.PI / 2)
         this.camera.up.set(0, 1, -1)
         this.camera.lookAt(this.stick.position)
         this.scene.add(this.stickPivot)
         
      })
   }

   // rotateAndRender([x, y, z]) {
   rotateAndRender(whole_reads) {
      // const RAD_TO_DEG = 57.29578
      let acc = whole_reads["acc_top"]
      let gyr = whole_reads["gyr_top"]
      let mag = whole_reads["mag_top"]
      
      
      // let latest_acc = latest(acc).map(v => v*0.0174533)
      // let roll = Math.atan2(latest_acc[1], latest_acc[2]) //* RAD_TO_DEG
      // let pitch = Math.atan2(-latest_acc[0], Math.sqrt(latest_acc[1]**2 + latest_acc[2]**2)) //* RAD_TO_DEG

      // let a_norm = l2norm(acc)
      // let pitchA = -Math.asin(x / a_norm)
      // let rollA = Math.asin(y / Math.cos(pitchA) / a_norm)

      // let latest_mag = latest(mag) 
      // let m_norm = l2norm(latest_mag)

      // let mx = latest_mag[0] / m_norm
      // let my = -latest_mag[1] / m_norm
      // let mz = latest_mag[2] / m_norm

      // let Mx = mx*Math.cos(pitchA) + mz*Math.sin(pitchA)
      // let My = mx*Math.sin(rollA)*Math.sin(pitchA) + my*Math.cos(rollA) - mz*Math.sin(rollA)*Math.cos(pitchA)
      // let yaw = Math.atan2(-My, Mx)
      
      // if (yaw > 360) {
      //    yaw -= 360
      // } else if (yaw < 0) {
      //    yaw += 360
      // }
      

      // https://stanford.edu/class/ee267/lectures/lecture9.pdf
      // let acc_latest = latest(acc)
      // let acc_norm = l2norm(acc_latest)
      // // Normalize
      // let [ax, ay, az] = acc_latest.map(v => v / acc_norm)
      
      // // teta_z is pitch that maps x axis on my model
      // const teta_z = -1.0 * -Math.atan2(-ax, ay)  // I add minus because of rendering. This is in radians [-pi, pi] 
      // // teta_x is roll that maps z axis on my model
      // const teta_x = -Math.atan2(az, Math.sign(ay)*Math.sqrt(ax**2 + ay**2))  // in radians [-pi, pi]
      // this.stick.rotation.x = teta_z
      // // this.stick.rotation.y = teta_z // apply roll_z to y because of the model render
      // this.stick.rotation.z = teta_x

      // Following arduino dashboard method
      let [x, y, z] = latest(acc)
      x *= 0.0174533 * -1.0
      y *= 0.0174533
      z *= 0.0174533
      let pitch = Math.atan2((-x), Math.sqrt(y*y + z*z))
      let roll = Math.atan2(y, z)
      this.stick.rotation.y = pitch
      this.stick.rotation.z = roll
      this.renderer.render(this.scene, this.camera)
   }

   rotateAndRenderGyro([x, y, z]) {
      // Use only gyroscope to compute angles
      x *= 0.0174533
      y *= 0.0174533
      z *= 0.0174533
      this.stickPivot.rotation.x = y
      this.stickPivot.rotation.y = -z
      this.stickPivot.rotation.z = x
      this.renderer.render(this.scene, this.camera)
   }

   // animate() {
   //    requestAnimationFrame(this.animate)
   //    this.rotateAndRender()
   // }

   getRenderer() {
      return this.renderer
   }

   getCamera() {
      return this.camera
   }

   getScene() {
      return this.scene
   }
}

export default new RenderStick({})