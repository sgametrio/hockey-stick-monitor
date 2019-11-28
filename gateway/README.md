# web-bluetooth-arduino

## Project setup
```
npm install
```

### Compiles and hot-reloads for development
```
npm run serve
```

### Compiles and minifies for production
```
npm run build
```

### Customize configuration
See [Configuration Reference](https://cli.vuejs.org/config/).

## Characteristics on Arduino
* management: (1 byte) write 0x01 to start recording, write 0x00 to stop it (default 0x00)
* data: (12 byte x sensor) read-only, 4 byte x value (x, y, z)

## Communicate with server
Look at `server/postman.py`
