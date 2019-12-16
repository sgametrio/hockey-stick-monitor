<template>
   <svg
      width="24"
      height="24"
      stroke="currentColor"
      :fill="fillString"
      :stroke-width="stroke"
      stroke-linecap="round"
      stroke-linejoin="round"
      xmlns="http://www.w3.org/2000/svg"
      xmlns:xlink="http://www.w3.org/1999/xlink"
      version="1.1"
   >
      <use :href="path" :key="path"/>
   </svg>
</template>

<script>
export default {
   name: "ColorSvg",
   props: {
      icon: {
         required: true,
         type: String
      },
      stroke: {
         type: String
      },
      fill: {
         type: String
      }
   },
   watch: {
   },
   computed: {
      path () {
         return `img/icons/feather-sprite.svg#${this.icon}`
      },
      fillString () {
         if (!this.fill) return "none"
         return "currentColor"
      }
   }
}
</script>